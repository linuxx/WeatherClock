#include "DisplayService.h"

#include <Adafruit_GFX.h>
#include <time.h>
#include "WeatherIcons.h"

DisplayService::DisplayService(Adafruit_SSD1306& display) : display_(display) {}

const char* DisplayService::shortWeatherLabel(WeatherType type) {
  return weatherTypeLabel(type);
}

const char* DisplayService::shortDayName(uint8_t dow) {
  static const char* names[] = {"Sun", "Mon", "Tues", "Wed", "Thur", "Fri", "Sat"};
  if (dow > 6) {
    return "Unknown";
  }
  return names[dow];
}

const char* DisplayService::windDirectionLabel(uint16_t deg) {
  static const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  const uint8_t idx = static_cast<uint8_t>(((deg + 22) % 360) / 45);
  return dirs[idx];
}

String DisplayService::formatHourLabel(uint8_t hour24) {
  const uint8_t h12 = (hour24 % 12 == 0) ? 12 : (hour24 % 12);
  const bool pm = hour24 >= 12;
  return String(h12) + (pm ? "p" : "a");
}

namespace {
// Current local hour from system clock (already adjusted via UTC offset in TimeService).
uint8_t currentLocalHour24() {
  time_t now = time(nullptr);
  tm localNow{};
  localtime_r(&now, &localNow);
  return static_cast<uint8_t>(localNow.tm_hour);
}

uint8_t currentLocalWday() {
  time_t now = time(nullptr);
  tm localNow{};
  localtime_r(&now, &localNow);
  return static_cast<uint8_t>(localNow.tm_wday);
}

// Picks which precomputed hourly slot is nearest to "now + 2h".
uint8_t pickHourlyStartIndex(const WeatherData& weather) {
  const uint8_t targetHour = static_cast<uint8_t>((currentLocalHour24() + 2) % 24);
  uint8_t bestIndex = 0;
  uint8_t bestDelta = 24;
  for (uint8_t i = 0; i < 4; ++i) {
    const uint8_t slotHour = weather.hourlyHour24[i] % 24;
    const uint8_t delta = static_cast<uint8_t>((slotHour + 24 - targetHour) % 24);
    if (delta < bestDelta) {
      bestDelta = delta;
      bestIndex = i;
    }
  }
  return bestIndex;
}

// Picks daily slot that matches tomorrow's weekday.
uint8_t pickDailyStartIndex(const WeatherData& weather) {
  const uint8_t tomorrowWday = static_cast<uint8_t>((currentLocalWday() + 1) % 7);
  for (uint8_t i = 0; i < 4; ++i) {
    if ((weather.dailyDow[i] % 7) == tomorrowWday) {
      return i;
    }
  }
  return 0;
}
}  // namespace

void DisplayService::setNetworkActivity(bool active, uint8_t frame) {
  networkBusy_ = active;
  networkAnimFrame_ = frame;
}

void DisplayService::drawBootScreen() {
  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);

  // Two-color OLED friendly layout: title in top band, details in lower band.
  display_.fillRect(0, 0, kScreenWidth, kTopBandHeight, SSD1306_BLACK);
  display_.drawLine(0, kTopBandHeight, kScreenWidth, kTopBandHeight, SSD1306_WHITE);

  display_.setTextSize(1);
  display_.setCursor(22, 4);
  display_.print("Weather Clock");

  display_.drawRoundRect(8, 24, 112, 26, 4, SSD1306_WHITE);
  display_.setTextSize(1);
  display_.setCursor(20, 33);
  display_.print("Starting up...");
  display_.display();
}

void DisplayService::drawStatusScreen(const char* title, const String& line1, const String& line2, const String& line3) {
  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.print(title);
  display_.drawLine(0, 10, kScreenWidth, 10, SSD1306_WHITE);

  display_.setCursor(0, 16);
  display_.print(line1);
  display_.setCursor(0, 28);
  display_.print(line2);
  display_.setCursor(0, 40);
  display_.print(line3);
  display_.display();
}

void DisplayService::drawLayoutFrame(const ClockData& clock, const WeatherData& weather, bool showColon) {
  display_.clearDisplay();
  drawTopBand(clock, showColon);
  drawBottomBand(weather);
  display_.display();
}

void DisplayService::drawPage(uint8_t pageIndex, const ClockData& clock, const WeatherData& weather, bool showColon) {
  if (pageIndex == 0) {
    drawLayoutFrame(clock, weather, showColon);
    return;
  }

  display_.clearDisplay();
  display_.fillRect(0, 0, kScreenWidth, kTopBandHeight, SSD1306_BLACK);
  display_.drawLine(0, kTopBandHeight, kScreenWidth, kTopBandHeight, SSD1306_WHITE);
  display_.setTextColor(SSD1306_WHITE);
  display_.setTextSize(1);

  if (!weather.valid) {
    display_.setCursor(0, 4);
    display_.print("Weather Pages");
    display_.setCursor(0, 28);
    display_.print("API ERROR");
    display_.display();
    return;
  }

  switch (pageIndex) {
    case 1:
      display_.setCursor(0, 4);
      display_.print("Today");
      drawTodayPage(weather);
      break;
    case 2:
      display_.setCursor(0, 4);
      display_.print("Hourly");
      drawHourlyPage(weather);
      break;
    case 3:
      display_.setCursor(0, 4);
      display_.print("4-Day");
      drawFourDayPage(weather);
      break;
    case 4:
      display_.setCursor(0, 4);
      display_.print("Advisories");
      drawAdvisoriesPage(weather);
      break;
    case 5:
      display_.setCursor(0, 4);
      display_.print("Wind");
      drawWindPage(weather);
      break;
    default:
      drawLayoutFrame(clock, weather, showColon);
      return;
  }

  display_.display();
}

void DisplayService::drawNetworkActivityIcon(int16_t x, int16_t y) const {
  if (!networkBusy_) {
    return;
  }

  // Small animated upload/download glyph.
  display_.drawRoundRect(x, y, 18, 10, 2, SSD1306_WHITE);
  if (networkAnimFrame_ % 2 == 0) {
    display_.drawTriangle(x + 4, y + 5, x + 8, y + 3, x + 8, y + 7, SSD1306_WHITE);
    display_.drawTriangle(x + 14, y + 5, x + 10, y + 3, x + 10, y + 7, SSD1306_WHITE);
  } else {
    display_.drawTriangle(x + 5, y + 3, x + 5, y + 7, x + 9, y + 5, SSD1306_WHITE);
    display_.drawTriangle(x + 13, y + 3, x + 13, y + 7, x + 9, y + 5, SSD1306_WHITE);
  }
}

void DisplayService::drawTopBand(const ClockData& clock, bool showColon) {
  display_.fillRect(0, 0, kScreenWidth, kTopBandHeight, SSD1306_BLACK);
  display_.drawLine(0, kTopBandHeight, kScreenWidth, kTopBandHeight, SSD1306_WHITE);

  if (!clock.valid) {
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextSize(1);
    display_.setCursor(30, 4);
    display_.print("NTP ERROR");
    return;
  }

  // 12-hour clock with blinking colon controlled by caller.
  char timeBuf[6];
  const uint8_t hour12 = (clock.hour % 12 == 0) ? 12 : (clock.hour % 12);
  const unsigned minute = static_cast<unsigned>(clock.minute % 60);
  snprintf(timeBuf, sizeof(timeBuf), showColon ? "%2u:%02u" : "%2u %02u", hour12, minute);

  display_.setTextColor(SSD1306_WHITE);
  display_.setTextSize(2);
  display_.setCursor(2, 0);
  display_.print(timeBuf);

  // Compact date at top-right.
  char dateBuf[6];
  const unsigned month = static_cast<unsigned>(clock.month % 100);
  const unsigned day = static_cast<unsigned>(clock.day % 100);
  snprintf(dateBuf, sizeof(dateBuf), "%u/%u", month, day);
  display_.setTextSize(1);
  display_.setCursor(88, 4);
  display_.print(dateBuf);
}

void DisplayService::drawBottomBand(const WeatherData& weather) {
  const int16_t y0 = kTopBandHeight + 2;
  const int16_t textRegionLeft = 40;
  const int16_t textRegionRight = kScreenWidth - 1;
  const int16_t textRegionWidth = textRegionRight - textRegionLeft + 1;

  // Utility to center a text string in the right-hand weather text column.
  auto centeredXForText = [&](const char* text) -> int16_t {
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    display_.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int16_t x = textRegionLeft + static_cast<int16_t>((textRegionWidth - static_cast<int16_t>(w)) / 2);
    if (x < textRegionLeft) {
      x = textRegionLeft;
    }
    return x;
  };

  if (!weather.valid) {
    drawNetworkActivityIcon(12, y0 + 34);
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextSize(1);
    display_.setCursor(44, y0 + 16);
    display_.print("API ERROR");
    return;
  }

  drawWeatherIcon(display_, weather.type, 2, y0 + 2);
  drawNetworkActivityIcon(12, y0 + 34);

  char tempBuf[12];
  snprintf(tempBuf, sizeof(tempBuf), "%dF", weather.temperatureF);
  const char* conditionText = weatherTypeLabel(weather.type);
  char rainBuf[16];
  snprintf(rainBuf, sizeof(rainBuf), "Rain %u%%", weather.rainChancePct);

  display_.setTextColor(SSD1306_WHITE);
  display_.setTextSize(2);
  display_.setCursor(centeredXForText(tempBuf), y0 + 2);
  display_.print(tempBuf);

  display_.setTextSize(1);
  display_.setCursor(centeredXForText(conditionText), y0 + 20);
  display_.print(conditionText);

  const int16_t precipitationY = y0 + 32;
  display_.setCursor(centeredXForText(rainBuf), precipitationY);
  display_.print(rainBuf);
}

void DisplayService::drawTodayPage(const WeatherData& weather) {
  display_.setTextSize(1);
  display_.setCursor(0, 18);
  display_.print("Feels Like ");
  display_.print(weather.feelsLikeF);
  display_.print("F");

  display_.setCursor(0, 28);
  display_.print("High ");
  display_.print(weather.todayHighF);
  display_.print("  Low ");
  display_.print(weather.todayLowF);

  display_.setCursor(0, 38);
  display_.print("Rise ");
  display_.print(weather.sunriseHour);
  display_.print(":");
  if (weather.sunriseMinute < 10) display_.print("0");
  display_.print(weather.sunriseMinute);
  display_.print("  Set ");
  display_.print(weather.sunsetHour);
  display_.print(":");
  if (weather.sunsetMinute < 10) display_.print("0");
  display_.print(weather.sunsetMinute);

  display_.setCursor(0, 48);
  display_.print("Wind ");
  display_.print(weather.windMph);
  display_.print(" mph");
}

void DisplayService::drawHourlyPage(const WeatherData& weather) {
  display_.setTextSize(1);
  // Rotates rows so first entry starts at +2h from current local time.
  const uint8_t start = pickHourlyStartIndex(weather);
  for (int i = 0; i < 4; ++i) {
    const uint8_t idx = static_cast<uint8_t>((start + i) % 4);
    const int16_t y = 20 + i * 11;
    display_.setCursor(0, y);
    display_.print(formatHourLabel(weather.hourlyHour24[idx]));
    display_.setCursor(34, y);
    display_.print(weather.hourlyTempF[idx]);
    display_.print("F");
    display_.setCursor(62, y);
    if (weather.hourlyMain[idx][0] != '\0') {
      display_.print(weather.hourlyMain[idx]);
    } else {
      display_.print(shortWeatherLabel(weather.hourlyType[idx]));
    }
  }
}

void DisplayService::drawFourDayPage(const WeatherData& weather) {
  display_.setTextSize(1);
  // Rows represent tomorrow through +3 days.
  const uint8_t start = pickDailyStartIndex(weather);
  const uint8_t baseWday = static_cast<uint8_t>((currentLocalWday() + 1) % 7);
  for (int i = 0; i < 4; ++i) {
    const uint8_t idx = static_cast<uint8_t>((start + i) % 4);
    const int16_t y = 20 + i * 11;
    display_.setCursor(0, y);
    display_.print(shortDayName(static_cast<uint8_t>((baseWday + i) % 7)));
    display_.setCursor(30, y);
    display_.print(weather.dailyHighF[idx]);
    display_.print("/");
    display_.print(weather.dailyLowF[idx]);
    display_.setCursor(66, y);
    if (weather.dailyMain[idx][0] != '\0') {
      display_.print(weather.dailyMain[idx]);
    } else {
      display_.print(shortWeatherLabel(weather.dailyType[idx]));
    }
  }
}

void DisplayService::drawAdvisoriesPage(const WeatherData& weather) {
  display_.setTextSize(1);
  display_.setCursor(0, 20);
  display_.print(weather.advisory);
}

void DisplayService::drawWindPage(const WeatherData& weather) {
  const uint8_t idx = static_cast<uint8_t>(((weather.windDeg + 22) % 360) / 45);
  static const int8_t dx[8] = {0, 7, 10, 7, 0, -7, -10, -7};
  static const int8_t dy[8] = {-10, -7, 0, 7, 10, 7, 0, -7};

  display_.setTextSize(1);
  display_.setCursor(0, 20);
  display_.print("Wind ");
  display_.print(weather.windMph);
  display_.print(" mph");
  display_.setCursor(0, 32);
  display_.print("Gust ");
  display_.print(weather.gustMph);
  display_.print(" mph");

  // Direction indicator drawn as vector on a compass ring.
  const int16_t cx = 101;
  const int16_t cy = 30;
  const int16_t ex = cx + dx[idx];
  const int16_t ey = cy + dy[idx];
  const int16_t bx = ex - dx[idx] / 2;
  const int16_t by = ey - dy[idx] / 2;
  const int16_t px = -dy[idx] / 2;
  const int16_t py = dx[idx] / 2;

  display_.drawCircle(cx, cy, 12, SSD1306_WHITE);
  display_.drawLine(cx, cy, ex, ey, SSD1306_WHITE);
  display_.drawLine(ex, ey, bx + px, by + py, SSD1306_WHITE);
  display_.drawLine(ex, ey, bx - px, by - py, SSD1306_WHITE);
  const char* dirLabel = windDirectionLabel(weather.windDeg);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  display_.getTextBounds(dirLabel, 0, 0, &x1, &y1, &w, &h);
  int16_t labelX = cx - static_cast<int16_t>(w / 2);
  if (labelX < 0) {
    labelX = 0;
  }
  display_.setCursor(labelX, 44);
  display_.print(dirLabel);
}
