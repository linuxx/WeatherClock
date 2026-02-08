#include <Arduino.h>
#include <string.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <Adafruit_SSD1306.h>
#include "DisplayService.h"
#include "OpenWeatherConfigService.h"
#include "OpenWeatherService.h"
#include "TimeService.h"

namespace {
// Display and UI timing constants.
constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr int8_t OLED_RESET = -1;
constexpr uint8_t OLED_ADDR = 0x3C;
constexpr uint8_t RESET_BUTTON_PIN = D5;
constexpr unsigned long RESET_HOLD_WINDOW_MS = 5000;
constexpr uint8_t TOTAL_PAGES = 6;  // 0=Home, 1..5 detail pages
constexpr unsigned long PAGE_AUTO_RETURN_MS = 10000;
constexpr unsigned long PAGE_BUTTON_DEBOUNCE_MS = 35;
const char* WIFI_MENU_WITH_SETTINGS[] = {"wifi", "param", "info", "exit"};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DisplayService displayService(display);
TimeService timeService;
OpenWeatherConfigService openWeatherConfigService;
OpenWeatherService openWeatherService(openWeatherConfigService);
WiFiManager wifiManager;
String deviceName;
String portalSsid;
bool networkBusy = false;
uint8_t networkAnimFrame = 0;
bool webPortalRunning = false;
bool pendingConfigSync = false;
DisplayService* configPortalDisplay = nullptr;

ClockData clockData{12, 0, 1, 1, false};
WeatherData currentWeather{
    67, 67, 0, 0, WeatherType::PartlyCloudy,
    67, 45, 7, 0, 17, 0, 0, 0, 0, "NO ADVISORIES",
    {12, 14, 16, 18},
    {67, 68, 69, 68},
    {WeatherType::PartlyCloudy, WeatherType::Cloudy, WeatherType::Cloudy, WeatherType::PartlyCloudy},
    {"Clouds", "Clouds", "Clouds", "Clouds"},
    {1, 2, 3, 4},
    {67, 68, 69, 70},
    {45, 46, 47, 48},
    {WeatherType::Cloudy, WeatherType::Clear, WeatherType::PartlyCloudy, WeatherType::Cloudy},
    {"Clouds", "Clear", "Clouds", "Clouds"},
    false};

String buildDeviceName() {
  // Use last 2 MAC bytes for a short unique suffix.
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
  return String("Wethr-") + suffix;
}

bool performHourlySync();
void showSyncStatus(const char* title, const String& line1, const String& line2, const String& line3);

void onParamsSaved() {
  Serial.println("[CFG] Params saved from portal");
  openWeatherConfigService.applyFromConfig();
  Serial.print("[CFG] ZIP='");
  Serial.print(openWeatherConfigService.zipCode());
  Serial.print("' API key length=");
  Serial.println(static_cast<int>(strlen(openWeatherConfigService.apiKey())));
  timeService.refreshClockData(clockData);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[CFG] Running immediate sync after config save");
    performHourlySync();
  } else {
    Serial.println("[CFG] WiFi not connected, queueing sync after config save");
    pendingConfigSync = true;
  }
}

void clearSavedAppSettings() {
  // Clear both network credentials and app-level OpenWeather settings.
  wifiManager.resetSettings();
  WiFi.disconnect(true);
  openWeatherConfigService.clearSaved();
}

bool shouldEnterFactoryResetFromButton() {
  // Hold reset button during boot countdown to wipe credentials/settings.
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  const unsigned long start = millis();
  int lastShownSeconds = -1;
  while (millis() - start < RESET_HOLD_WINDOW_MS) {
    const unsigned long elapsedMs = millis() - start;
    const int secondsRemaining = static_cast<int>((RESET_HOLD_WINDOW_MS - elapsedMs + 999) / 1000);
    if (secondsRemaining != lastShownSeconds) {
      lastShownSeconds = secondsRemaining;
      const String countdownText = String("") + secondsRemaining + " sec";
      Serial.print("[BOOT] Reset countdown: ");
      Serial.print(secondsRemaining);
      Serial.println(" sec remaining");
      displayService.drawStatusScreen(
          "Boot Options",
          "Push button for reset",
          "",
          countdownText);
    }

    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
      delay(30);
      if (digitalRead(RESET_BUTTON_PIN) == LOW) {
        return true;
      }
    }
    delay(20);
  }
  return false;
}

bool performHourlySync() {
  // Sync NTP first, then weather. Keep one combined status for the UI.
  Serial.println("[SYNC] Starting hourly sync");
  networkBusy = true;
  const bool ntpSynced = timeService.syncFromNtp();
  const bool clockRefreshed = ntpSynced && timeService.refreshClockData(clockData);
  if (!clockRefreshed) {
    clockData.valid = false;
    Serial.println("[SYNC] Clock refresh failed");
  }

  const bool weatherUpdated = openWeatherService.refreshWeather(currentWeather, nullptr);
  if (!weatherUpdated) {
    currentWeather.valid = false;
    Serial.println("[SYNC] Weather refresh failed");
  } else {
    const int32_t offset = openWeatherService.detectedUtcOffsetSeconds();
    Serial.print("[SYNC] Applying API timezone offset: ");
    Serial.println(offset);
    timeService.setUtcOffsetSeconds(offset);
    timeService.refreshClockData(clockData);
  }

  networkBusy = false;
  Serial.print("[SYNC] Completed. time=");
  Serial.print(clockRefreshed ? "ok" : "error");
  Serial.print(" weather=");
  Serial.println(weatherUpdated ? "ok" : "error");
  return clockRefreshed && weatherUpdated;
}

void showSyncStatus(const char* title, const String& line1, const String& line2, const String& line3) {
  displayService.drawStatusScreen(title, line1, line2, line3);
}

bool handlePageButtonClick(unsigned long now, uint8_t& pageIndex, unsigned long& lastPageInteractionMs) {
  // Debounced button click: LOW edge advances page.
  static int lastRawState = HIGH;
  static int stableState = HIGH;
  static unsigned long lastDebounceMs = 0;

  const int rawState = digitalRead(RESET_BUTTON_PIN);
  if (rawState != lastRawState) {
    lastDebounceMs = now;
    lastRawState = rawState;
  }

  if (now - lastDebounceMs < PAGE_BUTTON_DEBOUNCE_MS) {
    return false;
  }

  if (stableState != rawState) {
    stableState = rawState;
    if (stableState == LOW) {
      pageIndex = static_cast<uint8_t>((pageIndex + 1) % TOTAL_PAGES);
      lastPageInteractionMs = now;
      Serial.print("[UI] Button click -> page ");
      Serial.println(pageIndex + 1);
      return true;
    }
  }

  return false;
}

void onConfigPortalStart(WiFiManager* wm) {
  (void)wm;
  if (configPortalDisplay != nullptr) {
    configPortalDisplay->drawStatusScreen(
        "WiFi Setup",
        String("Join: ") + portalSsid,
        "WiFi + Weather config",
        "192.168.4.1");
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("[BOOT] WeatherClock starting");

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;) {
      delay(1000);
    }
  }

  displayService.drawBootScreen();
  delay(500);

  WiFi.mode(WIFI_STA);
  deviceName = buildDeviceName();
  portalSsid = deviceName;
  Serial.print("[BOOT] Device name: ");
  Serial.println(deviceName);
  WiFi.hostname(deviceName);
  configPortalDisplay = &displayService;
  openWeatherConfigService.load();
  openWeatherConfigService.configurePortal(wifiManager);

  wifiManager.setAPCallback(onConfigPortalStart);
  wifiManager.setSaveParamsCallback(onParamsSaved);
  wifiManager.setParamsPage(false);
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setMenu(WIFI_MENU_WITH_SETTINGS, 4);
  wifiManager.setConnectRetries(5);
  wifiManager.setConnectTimeout(20);
  wifiManager.setConfigPortalTimeout(180);

  bool connected = false;
  if (shouldEnterFactoryResetFromButton()) {
    Serial.println("[BOOT] Reset button pressed, entering config portal");
    displayService.drawStatusScreen("Reset", "Clearing WiFi + app cfg", "Starting config", portalSsid);
    clearSavedAppSettings();
    connected = wifiManager.startConfigPortal(portalSsid.c_str());
  } else {
    Serial.println("[BOOT] Attempting autoConnect");
    displayService.drawStatusScreen("WiFi", "Trying saved network...", deviceName, "");
    connected = wifiManager.autoConnect(portalSsid.c_str());
  }

  if (!connected) {
    Serial.println("[WIFI] Connection failed, restarting");
    displayService.drawStatusScreen("WiFi Failed", "No network configured", "Restarting...", "");
    delay(2000);
    ESP.restart();
  }
  Serial.print("[WIFI] Connected. SSID=");
  Serial.print(WiFi.SSID());
  Serial.print(" IP=");
  Serial.println(WiFi.localIP());

  openWeatherConfigService.applyFromConfig();
  timeService.refreshClockData(clockData);

  displayService.drawStatusScreen(
      "WiFi Connected",
      deviceName,
      WiFi.SSID(),
      WiFi.localIP().toString());
  delay(800);

  displayService.drawStatusScreen(
      "Time",
      "Syncing NTP...",
      "Timezone from ZIP/API",
      "0.us.pool.ntp.org + backups");
  displayService.setNetworkActivity(true, networkAnimFrame);
  networkBusy = true;
  const bool ntpSynced = timeService.syncFromNtp();
  const bool clockRefreshed = ntpSynced && timeService.refreshClockData(clockData);
  if (!clockRefreshed) {
    clockData.valid = false;
  }
  const bool weatherUpdated = openWeatherService.refreshWeather(currentWeather, showSyncStatus);
  if (!weatherUpdated) {
    currentWeather.valid = false;
  } else {
    const int32_t offset = openWeatherService.detectedUtcOffsetSeconds();
    Serial.print("[BOOT] Applying API timezone offset: ");
    Serial.println(offset);
    timeService.setUtcOffsetSeconds(offset);
    timeService.refreshClockData(clockData);
  }
  networkBusy = false;

  // Keep WiFiManager web UI reachable at the station IP while normal app runs.
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.startWebPortal();
  webPortalRunning = true;

  displayService.drawLayoutFrame(clockData, currentWeather, true);
}

void loop() {
  // Keep UI fresh while pulling NTP/weather at boot and on every hour boundary.
  static unsigned long lastClockRefreshMs = 0;
  static unsigned long lastNoTimeRetryMs = 0;
  static unsigned long lastBlinkToggleMs = 0;
  static unsigned long lastNetworkAnimMs = 0;
  static long lastHourlySyncKey = -1;
  static bool showColon = true;
  static uint8_t currentPage = 0;
  static unsigned long lastPageInteractionMs = 0;
  const unsigned long now = millis();

  if (now - lastClockRefreshMs >= 1000) {
    lastClockRefreshMs = now;
    if (!timeService.refreshClockData(clockData)) {
      clockData.valid = false;
    }
  }

  if (now - lastBlinkToggleMs >= 500) {
    lastBlinkToggleMs = now;
    showColon = !showColon;
  }

  if (clockData.valid) {
    const long hourlySyncKey =
        static_cast<long>(clockData.month) * 100000L + static_cast<long>(clockData.day) * 1000L + clockData.hour;
    if (clockData.minute == 0 && hourlySyncKey != lastHourlySyncKey) {
      Serial.println("[SYNC] Top of hour reached, syncing");
      performHourlySync();
      lastHourlySyncKey = hourlySyncKey;
    }
  } else if (now - lastNoTimeRetryMs >= 60000) {
    // If time is invalid, retry once per minute until NTP returns.
    Serial.println("[SYNC] Time invalid, running retry sync");
    lastNoTimeRetryMs = now;
    performHourlySync();
  }

  if (networkBusy && now - lastNetworkAnimMs >= 250) {
    lastNetworkAnimMs = now;
    networkAnimFrame++;
  }

  if (webPortalRunning) {
    wifiManager.process();
  }

  handlePageButtonClick(now, currentPage, lastPageInteractionMs);
  if (currentPage != 0 && now - lastPageInteractionMs >= PAGE_AUTO_RETURN_MS) {
    currentPage = 0;
  }

  if (pendingConfigSync && WiFi.status() == WL_CONNECTED) {
    Serial.println("[CFG] Processing queued sync");
    pendingConfigSync = false;
    performHourlySync();
  }

  displayService.setNetworkActivity(networkBusy, networkAnimFrame);
  displayService.drawPage(currentPage, clockData, currentWeather, showColon);
}
