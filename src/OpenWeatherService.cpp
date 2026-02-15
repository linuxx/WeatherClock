#include "OpenWeatherService.h"

#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
using SecureClient = BearSSL::WiFiClientSecure;
#elif defined(ARDUINO_ARCH_ESP32)
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
using SecureClient = WiFiClientSecure;
#else
#error Unsupported architecture: expected ESP8266 or ESP32
#endif
#include <string.h>
#include <time.h>

namespace {
// Converts probability [0..1] into integer percent [0..100].
int clampToPercent(double pop) {
  if (pop < 0) {
    pop = 0;
  }
  if (pop > 1) {
    pop = 1;
  }
  return static_cast<int>(pop * 100.0 + 0.5);
}

// Finds the matching closing brace index for an object starting at openPos.
int findMatchingBrace(const String& text, int openPos) {
  if (openPos < 0 || openPos >= static_cast<int>(text.length()) || text[openPos] != '{') {
    return -1;
  }

  int depth = 0;
  for (int i = openPos; i < static_cast<int>(text.length()); ++i) {
    if (text[i] == '{') {
      ++depth;
    } else if (text[i] == '}') {
      --depth;
      if (depth == 0) {
        return i;
      }
    }
  }
  return -1;
}

// Finds the matching closing bracket index for an array starting at openPos.
int findMatchingBracket(const String& text, int openPos) {
  if (openPos < 0 || openPos >= static_cast<int>(text.length()) || text[openPos] != '[') {
    return -1;
  }

  int depth = 0;
  for (int i = openPos; i < static_cast<int>(text.length()); ++i) {
    if (text[i] == '[') {
      ++depth;
    } else if (text[i] == ']') {
      --depth;
      if (depth == 0) {
        return i;
      }
    }
  }
  return -1;
}

// Reads a numeric field allowing optional whitespace after ':'.
bool parseFieldNumberFlexible(const String& json, const char* fieldName, double& outValue) {
  if (fieldName == nullptr) {
    return false;
  }

  const int keyPos = json.indexOf(fieldName);
  if (keyPos < 0) {
    return false;
  }

  const int colonPos = json.indexOf(':', keyPos + static_cast<int>(strlen(fieldName)));
  if (colonPos < 0) {
    return false;
  }

  int valueStart = colonPos + 1;
  while (valueStart < static_cast<int>(json.length()) &&
         (json[valueStart] == ' ' || json[valueStart] == '\t' || json[valueStart] == '\r' ||
          json[valueStart] == '\n')) {
    ++valueStart;
  }

  int valueEnd = valueStart;
  while (valueEnd < static_cast<int>(json.length())) {
    const char c = json[valueEnd];
    const bool numericChar = (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
    if (!numericChar) {
      break;
    }
    ++valueEnd;
  }

  if (valueEnd <= valueStart) {
    return false;
  }

  outValue = json.substring(valueStart, valueEnd).toFloat();
  return true;
}

// Emits a normalized summary of parsed weather fields for serial debugging.
void logParsedWeather(const WeatherData& weather) {
  Serial.println("[OWM] ---- Parsed Weather ----");
  Serial.print("[OWM] Current: temp=");
  Serial.print(weather.temperatureF);
  Serial.print("F feels=");
  Serial.print(weather.feelsLikeF);
  Serial.print("F rain=");
  Serial.print(weather.rainChancePct);
  Serial.print("% wind=");
  Serial.print(weather.windMph);
  Serial.print("mph gust=");
  Serial.print(weather.gustMph);
  Serial.print("mph deg=");
  Serial.println(weather.windDeg);

  Serial.print("[OWM] Today: high=");
  Serial.print(weather.todayHighF);
  Serial.print(" low=");
  Serial.print(weather.todayLowF);
  Serial.print(" sunrise=");
  Serial.print(weather.sunriseHour);
  Serial.print(":");
  if (weather.sunriseMinute < 10) Serial.print("0");
  Serial.print(weather.sunriseMinute);
  Serial.print(" sunset=");
  Serial.print(weather.sunsetHour);
  Serial.print(":");
  if (weather.sunsetMinute < 10) Serial.print("0");
  Serial.println(weather.sunsetMinute);

  for (int i = 0; i < 4; ++i) {
    Serial.print("[OWM] Hourly[");
    Serial.print(i);
    Serial.print("]: h=");
    Serial.print(weather.hourlyHour24[i]);
    Serial.print(" temp=");
    Serial.print(weather.hourlyTempF[i]);
    Serial.print(" main=");
    Serial.print(weather.hourlyMain[i]);
    Serial.print(" type=");
    Serial.println(static_cast<int>(weather.hourlyType[i]));
  }

  for (int i = 0; i < 4; ++i) {
    Serial.print("[OWM] Daily[");
    Serial.print(i);
    Serial.print("]: dow=");
    Serial.print(weather.dailyDow[i]);
    Serial.print(" high=");
    Serial.print(weather.dailyHighF[i]);
    Serial.print(" low=");
    Serial.print(weather.dailyLowF[i]);
    Serial.print(" main=");
    Serial.print(weather.dailyMain[i]);
    Serial.print(" type=");
    Serial.println(static_cast<int>(weather.dailyType[i]));
  }

  Serial.print("[OWM] Advisory: ");
  Serial.println(weather.advisory);
  Serial.println("[OWM] -----------------------");
}

}  // namespace

// Initializes parser/fetch service with config source and empty derived state.
OpenWeatherService::OpenWeatherService(const OpenWeatherConfigService& configService)
    : configService_(configService) {
  lastLocationName_[0] = '\0';
  detectedUtcOffsetSeconds_ = 0;
}

// Finds a JSON section by key and returns its index or -1.
int OpenWeatherService::findSection(const String& json, const char* sectionKey) {
  return json.indexOf(sectionKey);
}

// Rounds floating-point value to nearest integer using half-away-from-zero behavior.
int OpenWeatherService::roundToInt(double value) {
  return static_cast<int>(value + (value >= 0 ? 0.5 : -0.5));
}

// Parses a numeric field from the root payload.
bool OpenWeatherService::parseNumber(const String& json, const char* key, double& outValue) {
  return parseNumberFrom(json, key, 0, outValue, nullptr);
}

// Parses an integer field from the root payload.
bool OpenWeatherService::parseInt(const String& json, const char* key, int& outValue) {
  return parseIntFrom(json, key, 0, outValue, nullptr);
}

// Parses a numeric field by key from an offset and optionally returns end position.
bool OpenWeatherService::parseNumberFrom(
    const String& json, const char* key, int start, double& outValue, int* valuePos) {
  if (key == nullptr || start < 0) {
    return false;
  }

  const int keyPosInt = json.indexOf(key, start);
  if (keyPosInt < 0) {
    return false;
  }

  const size_t keyPos = static_cast<size_t>(keyPosInt);
  const size_t jsonLen = json.length();
  size_t valueStart = keyPos + strlen(key);
  while (valueStart < jsonLen &&
         (json[valueStart] == ' ' || json[valueStart] == '\t' || json[valueStart] == '\r' ||
          json[valueStart] == '\n')) {
    ++valueStart;
  }

  size_t valueEnd = valueStart;
  while (valueEnd < jsonLen) {
    const char c = json[valueEnd];
    const bool numericChar = (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
    if (!numericChar) {
      break;
    }
    ++valueEnd;
  }

  if (valueEnd <= valueStart) {
    return false;
  }

  const String numberText = json.substring(valueStart, valueEnd);
  outValue = numberText.toFloat();
  if (valuePos != nullptr) {
    *valuePos = static_cast<int>(valueEnd);
  }
  return true;
}

// Parses an integer field by key from an offset and optionally returns end position.
bool OpenWeatherService::parseIntFrom(const String& json, const char* key, int start, int& outValue, int* valuePos) {
  double parsed = 0;
  if (!parseNumberFrom(json, key, start, parsed, valuePos)) {
    return false;
  }
  outValue = static_cast<int>(parsed);
  return true;
}

// Parses a quoted string field by key from an offset.
bool OpenWeatherService::parseStringFrom(
    const String& json, const char* key, int start, char* outBuf, size_t outBufSize, int* valuePos) {
  if (key == nullptr || outBuf == nullptr || outBufSize == 0 || start < 0) {
    return false;
  }

  const int keyPos = json.indexOf(key, start);
  if (keyPos < 0) {
    return false;
  }

  int valueStart = keyPos + static_cast<int>(strlen(key));
  if (valueStart >= static_cast<int>(json.length())) {
    return false;
  }

  int valueEnd = valueStart;
  while (valueEnd < static_cast<int>(json.length()) && json[valueEnd] != '"') {
    ++valueEnd;
  }
  if (valueEnd <= valueStart) {
    return false;
  }

  const String text = json.substring(valueStart, valueEnd);
  strncpy(outBuf, text.c_str(), outBufSize - 1);
  outBuf[outBufSize - 1] = '\0';
  if (valuePos != nullptr) {
    *valuePos = valueEnd;
  }
  return true;
}

// Maps OpenWeather condition codes to local icon/text weather categories.
WeatherType OpenWeatherService::mapWeatherType(int weatherId) {
  if (weatherId >= 200 && weatherId < 300) {
    return WeatherType::Thunderstorm;
  }
  if (weatherId >= 300 && weatherId < 600) {
    return WeatherType::Rain;
  }
  if (weatherId >= 600 && weatherId < 700) {
    return WeatherType::Snow;
  }
  if (weatherId == 800) {
    return WeatherType::Clear;
  }
  if (weatherId == 801 || weatherId == 802) {
    return WeatherType::PartlyCloudy;
  }
  if (weatherId == 803 || weatherId == 804) {
    return WeatherType::Cloudy;
  }
  if (weatherId == 741 || (weatherId >= 700 && weatherId < 800)) {
    return WeatherType::Fog;
  }
  return WeatherType::Cloudy;
}

// Resolves configured ZIP code into latitude/longitude through OpenWeather geocoding.
bool OpenWeatherService::fetchCoordinatesForZip(
    const char* zipInput, const char* apiKey, double& lat, double& lon, ProgressCallback progress) const {
  if (progress != nullptr) {
    progress("Weather API", "Getting coordinates", String("ZIP: ") + zipInput, "");
  }
  const String geoUrl = String("https://api.openweathermap.org/geo/1.0/zip?zip=") +
                        zipInput + "&appid=" + apiKey;
  Serial.print("[OWM] Geocode request: ");
  Serial.println(geoUrl);
  SecureClient client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, geoUrl)) {
    Serial.println("[OWM] Geocode begin() failed");
    return false;
  }

  http.setTimeout(10000);
  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("[OWM] Geocode HTTP error: ");
    Serial.println(httpCode);
    const String body = http.getString();
    if (body.length() > 0) {
      Serial.print("[OWM] Geocode response: ");
      Serial.println(body);
    }
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  if (!parseNumber(payload, "\"lat\":", lat) || !parseNumber(payload, "\"lon\":", lon)) {
    Serial.println("[OWM] Failed to parse lat/lon from geocode payload");
    Serial.println(payload);
    return false;
  }

  if (!parseStringFrom(payload, "\"name\":\"", 0, lastLocationName_, sizeof(lastLocationName_), nullptr)) {
    strncpy(lastLocationName_, "Selected ZIP", sizeof(lastLocationName_) - 1);
    lastLocationName_[sizeof(lastLocationName_) - 1] = '\0';
  }

  Serial.print("[OWM] Geocode success lat/lon: ");
  Serial.print(lat, 6);
  Serial.print(", ");
  Serial.println(lon, 6);
  return true;
}

// Fetches and parses OneCall payload into display model fields.
bool OpenWeatherService::fetchWeatherByCoordinates(
    double lat, double lon, const char* apiKey, WeatherData& weather, ProgressCallback progress) const {
  if (progress != nullptr) {
    progress("Weather API", String("Getting weather for"), String(lastLocationName_), "");
  }
  // Small helper that fetches a OneCall payload with retry + full-length validation.
  auto fetchOneCallPayload = [&](const char* exclude, const char* tag, String& outPayload) -> bool {
    const String url = String("https://api.openweathermap.org/data/3.0/onecall?lat=") + String(lat, 6) +
                       "&lon=" + String(lon, 6) + "&units=imperial&exclude=" + exclude + "&appid=" + apiKey;
    for (int attempt = 1; attempt <= 2; ++attempt) {
      Serial.print("[OWM] ");
      Serial.print(tag);
      Serial.print(" request (attempt ");
      Serial.print(attempt);
      Serial.print("): ");
      Serial.println(url);

      SecureClient client;
      client.setInsecure();
      // Larger RX buffer helps prevent truncated reads on larger payloads.
#if defined(ARDUINO_ARCH_ESP8266)
      client.setBufferSizes(4096, 1024);
#endif

      HTTPClient http;
      if (!http.begin(client, url)) {
        Serial.print("[OWM] ");
        Serial.print(tag);
        Serial.println(" begin() failed");
        return false;
      }

      const char* headerKeys[] = {"Content-Type", "Content-Encoding", "Transfer-Encoding", "Content-Length"};
      http.collectHeaders(headerKeys, 4);
      http.useHTTP10(true);
      http.addHeader("Accept-Encoding", "identity");
      http.setTimeout(20000);
      const int httpCode = http.GET();
      if (httpCode != HTTP_CODE_OK) {
        Serial.print("[OWM] ");
        Serial.print(tag);
        Serial.print(" HTTP error: ");
        Serial.println(httpCode);
        const String body = http.getString();
        if (body.length() > 0) {
          Serial.print("[OWM] ");
          Serial.print(tag);
          Serial.print(" response: ");
          Serial.println(body);
        }
        http.end();
        if (attempt == 2) {
          return false;
        }
        delay(200);
        continue;
      }

      const int contentLength = http.getSize();
      Serial.print("[OWM] ");
      Serial.print(tag);
      Serial.print(" Content-Length: ");
      Serial.println(http.header("Content-Length"));
      Serial.print("[OWM] ");
      Serial.print(tag);
      Serial.print(" Content-Encoding: ");
      Serial.println(http.header("Content-Encoding"));
      Serial.print("[OWM] ");
      Serial.print(tag);
      Serial.print(" Transfer-Encoding: ");
      Serial.println(http.header("Transfer-Encoding"));

      constexpr int kMaxPayloadBytes = 30000;
      outPayload = "";
      if (contentLength > 0 && contentLength < kMaxPayloadBytes) {
        outPayload.reserve(static_cast<unsigned int>(contentLength + 64));
      } else {
        outPayload.reserve(kMaxPayloadBytes);
      }

      WiFiClient* stream = http.getStreamPtr();
      char buf[512];
      unsigned long lastReadMs = millis();
      while ((http.connected() || stream->available() > 0) && outPayload.length() < kMaxPayloadBytes) {
        int available = stream->available();
        if (available > 0) {
          int toRead = available;
          if (toRead > static_cast<int>(sizeof(buf))) {
            toRead = static_cast<int>(sizeof(buf));
          }
          const size_t got = stream->readBytes(buf, static_cast<size_t>(toRead));
          if (got > 0) {
            outPayload.concat(buf, got);
            lastReadMs = millis();
          }
        } else {
          if (millis() - lastReadMs > 30000) {
            break;
          }
          delay(1);
        }
      }
      http.end();

      Serial.print("[OWM] ");
      Serial.print(tag);
      Serial.print(" payload length=");
      Serial.println(outPayload.length());

      if (contentLength > 0 && outPayload.length() > 0 &&
          static_cast<int>(outPayload.length()) != contentLength) {
        Serial.print("[OWM] ");
        Serial.print(tag);
        Serial.print(" partial payload: got ");
        Serial.print(outPayload.length());
        Serial.print(" expected ");
        Serial.println(contentLength);
        if (attempt == 2) {
          return false;
        }
        delay(200);
        continue;
      }

      return outPayload.length() > 0;
    }

    return false;
  };

  String corePayload;
  if (!fetchOneCallPayload("minutely,alerts", "OneCall(core)", corePayload)) {
    return false;
  }

  // Defaults
  weather.rainChancePct = 0;
  weather.snowChancePct = 0;
  weather.feelsLikeF = 0;
  weather.todayHighF = 0;
  weather.todayLowF = 0;
  weather.sunriseHour = 0;
  weather.sunriseMinute = 0;
  weather.sunsetHour = 0;
  weather.sunsetMinute = 0;
  weather.windMph = 0;
  weather.gustMph = 0;
  weather.windDeg = 0;
  strncpy(weather.advisory, "NO ADVISORIES", sizeof(weather.advisory) - 1);
  weather.advisory[sizeof(weather.advisory) - 1] = '\0';
  for (int i = 0; i < 4; ++i) {
    weather.hourlyHour24[i] = 0;
    weather.hourlyTempF[i] = 0;
    weather.hourlyType[i] = WeatherType::Cloudy;
    weather.hourlyMain[i][0] = '\0';
    weather.dailyDow[i] = 0;
    weather.dailyHighF[i] = 0;
    weather.dailyLowF[i] = 0;
    weather.dailyType[i] = WeatherType::Cloudy;
    weather.dailyMain[i][0] = '\0';
  }

  int timezoneOffsetSec = 0;
  parseInt(corePayload, "\"timezone_offset\":", timezoneOffsetSec);
  char timezoneName[40] = {0};
  parseStringFrom(corePayload, "\"timezone\":\"", 0, timezoneName, sizeof(timezoneName), nullptr);
  detectedUtcOffsetSeconds_ = timezoneOffsetSec;
  Serial.print("[OWM] Timezone from API: iana='");
  Serial.print(timezoneName);
  Serial.print("' offsetSec=");
  Serial.print(timezoneOffsetSec);
  Serial.println("'");

  const int currentPos = findSection(corePayload, "\"current\":");
  double currentTemp = 0;
  int currentId = 0;
  const int currentStart = (currentPos >= 0) ? currentPos : 0;
  if (!parseNumberFrom(corePayload, "\"temp\":", currentStart, currentTemp, nullptr) ||
      !(parseIntFrom(corePayload, "\"weather\":[{\"id\":", currentStart, currentId, nullptr) ||
        parseIntFrom(corePayload, "\"id\":", currentStart, currentId, nullptr))) {
    Serial.println("[OWM] Failed to parse current temp/weather id");
    Serial.print("[OWM] currentStart=");
    Serial.println(currentStart);
    Serial.print("[OWM] payload head: ");
    Serial.println(corePayload.substring(0, 220));
    return false;
  }
  weather.temperatureF = static_cast<int16_t>(roundToInt(currentTemp));
  weather.type = mapWeatherType(currentId);
  weather.feelsLikeF = weather.temperatureF;
  double currentFeelsLike = 0;
  if (parseNumberFrom(corePayload, "\"feels_like\":", currentStart, currentFeelsLike, nullptr)) {
    weather.feelsLikeF = static_cast<int16_t>(roundToInt(currentFeelsLike));
  }
  weather.todayHighF = weather.temperatureF;
  weather.todayLowF = weather.temperatureF;
  double windSpeed = 0;
  if (parseNumberFrom(corePayload, "\"wind_speed\":", currentStart, windSpeed, nullptr)) {
    weather.windMph = static_cast<uint8_t>(roundToInt(windSpeed));
  }
  double windGust = 0;
  if (parseNumberFrom(corePayload, "\"wind_gust\":", currentStart, windGust, nullptr)) {
    weather.gustMph = static_cast<uint8_t>(roundToInt(windGust));
  }
  int windDeg = 0;
  if (parseIntFrom(corePayload, "\"wind_deg\":", currentStart, windDeg, nullptr)) {
    if (windDeg < 0) windDeg = 0;
    if (windDeg > 359) windDeg = windDeg % 360;
    weather.windDeg = static_cast<uint16_t>(windDeg);
  }

  int sunrise = 0;
  if (parseIntFrom(corePayload, "\"sunrise\":", currentStart, sunrise, nullptr)) {
    time_t localSunrise = static_cast<time_t>(sunrise + timezoneOffsetSec);
    tm sunriseInfo{};
    gmtime_r(&localSunrise, &sunriseInfo);
    weather.sunriseHour = static_cast<uint8_t>(sunriseInfo.tm_hour);
    weather.sunriseMinute = static_cast<uint8_t>(sunriseInfo.tm_min);
  }

  int sunset = 0;
  if (parseIntFrom(corePayload, "\"sunset\":", currentStart, sunset, nullptr)) {
    time_t localSunset = static_cast<time_t>(sunset + timezoneOffsetSec);
    tm sunsetInfo{};
    gmtime_r(&localSunset, &sunsetInfo);
    weather.sunsetHour = static_cast<uint8_t>(sunsetInfo.tm_hour);
    weather.sunsetMinute = static_cast<uint8_t>(sunsetInfo.tm_min);
  }

  const int hourlyPos = findSection(corePayload, "\"hourly\":");
  if (hourlyPos >= 0) {
    // Precip probability from first hourly entry.
    double pop = 0;
    if (parseNumberFrom(corePayload, "\"pop\":", hourlyPos, pop, nullptr)) {
      weather.rainChancePct = static_cast<uint8_t>(clampToPercent(pop));
    }

    // Build hourly targets from the current local hour (rounded down to hh:00),
    // so rows remain +2h, +4h, +6h, +8h regardless of provider array offsets.
    time_t targetLocalEpoch[4] = {0, 0, 0, 0};
    int nextTarget = 0;
    const time_t utcNow = time(nullptr);
    if (utcNow >= 8 * 3600 * 2) {
      const time_t localNow = static_cast<time_t>(utcNow + timezoneOffsetSec);
      const time_t baseHour = localNow - (localNow % 3600);
      for (int i = 0; i < 4; ++i) {
        targetLocalEpoch[i] = baseHour + static_cast<time_t>((i + 1) * 2 * 3600);
      }
    }

    int parsePos = hourlyPos;
    int hourlyIndex = 0;  // Fallback index-based selection if system time is not valid.
    int selectedFallback = 0;
    const int wanted[4] = {2, 4, 6, 8};
    while (hourlyIndex < 96 && (nextTarget < 4 || selectedFallback < 4)) {
      const int objStart = corePayload.indexOf('{', parsePos);
      if (objStart < 0) {
        break;
      }
      const int objEnd = findMatchingBrace(corePayload, objStart);
      if (objEnd < 0) {
        break;
      }
      const String hourJson = corePayload.substring(objStart, objEnd + 1);
      parsePos = objEnd + 1;

      int dt = 0;
      if (!parseIntFrom(hourJson, "\"dt\":", 0, dt, nullptr)) {
        continue;
      }

      double hourTemp = currentTemp;
      parseNumberFrom(hourJson, "\"temp\":", 0, hourTemp, nullptr);
      int hourId = currentId;
      parseIntFrom(hourJson, "\"id\":", 0, hourId, nullptr);
      char hourMain[12] = {0};
      parseStringFrom(hourJson, "\"main\":\"", 0, hourMain, sizeof(hourMain), nullptr);

      const time_t localDt = static_cast<time_t>(dt + timezoneOffsetSec);
      bool selectedThisEntry = false;
      int targetSlot = -1;

      if (targetLocalEpoch[0] != 0) {
        if (nextTarget < 4 && localDt >= targetLocalEpoch[nextTarget]) {
          targetSlot = nextTarget;
          ++nextTarget;
          selectedThisEntry = true;
        }
      } else if (selectedFallback < 4 && hourlyIndex == wanted[selectedFallback]) {
        targetSlot = selectedFallback;
        ++selectedFallback;
        selectedThisEntry = true;
      }

      if (selectedThisEntry && targetSlot >= 0 && targetSlot < 4) {
        tm hourInfo{};
        gmtime_r(&localDt, &hourInfo);
        weather.hourlyHour24[targetSlot] = static_cast<uint8_t>(hourInfo.tm_hour);
        weather.hourlyTempF[targetSlot] = static_cast<int16_t>(roundToInt(hourTemp));
        weather.hourlyType[targetSlot] = mapWeatherType(hourId);
        strncpy(weather.hourlyMain[targetSlot], hourMain, sizeof(weather.hourlyMain[targetSlot]) - 1);
        weather.hourlyMain[targetSlot][sizeof(weather.hourlyMain[targetSlot]) - 1] = '\0';
      }

      hourlyIndex++;
    }
  }

  const int alertsPos = findSection(corePayload, "\"alerts\":");
  if (alertsPos >= 0) {
    if (!parseStringFrom(corePayload, "\"event\":\"", alertsPos, weather.advisory, sizeof(weather.advisory), nullptr)) {
      strncpy(weather.advisory, "ADVISORY ACTIVE", sizeof(weather.advisory) - 1);
      weather.advisory[sizeof(weather.advisory) - 1] = '\0';
    }
  } else if (weather.gustMph >= 20) {
    strncpy(weather.advisory, "WIND ADVISORY", sizeof(weather.advisory) - 1);
    weather.advisory[sizeof(weather.advisory) - 1] = '\0';
  }

  // Parse daily directly from the same OneCall payload, preserving array order.
  struct ParsedDailyItem {
    uint8_t dow;
    int16_t high;
    int16_t low;
    WeatherType type;
    char main[12];
  };
  ParsedDailyItem parsedDaily[8]{};
  int parsedDailyCount = 0;

  const int dailyKeyPos = findSection(corePayload, "\"daily\":");
  if (dailyKeyPos >= 0) {
    const int arrayStart = corePayload.indexOf('[', dailyKeyPos);
    const int arrayEnd = findMatchingBracket(corePayload, arrayStart);
    if (arrayStart >= 0 && arrayEnd > arrayStart) {
      int parsePos = arrayStart + 1;
      while (parsePos < arrayEnd && parsedDailyCount < 8) {
        const int objStart = corePayload.indexOf('{', parsePos);
        if (objStart < 0 || objStart > arrayEnd) {
          break;
        }
        const int objEnd = findMatchingBrace(corePayload, objStart);
        if (objEnd < 0 || objEnd > arrayEnd) {
          break;
        }
        const String dayJson = corePayload.substring(objStart, objEnd + 1);
        parsePos = objEnd + 1;

        int dt = 0;
        if (!parseIntFrom(dayJson, "\"dt\":", 0, dt, nullptr)) {
          continue;
        }

        double maxTemp = currentTemp;
        double minTemp = currentTemp;
        bool hasMax = false;
        bool hasMin = false;
        const int tempKeyPos = dayJson.indexOf("\"temp\":");
        if (tempKeyPos >= 0) {
          const int tempObjStart = dayJson.indexOf('{', tempKeyPos);
          if (tempObjStart >= 0) {
            const int tempObjEnd = findMatchingBrace(dayJson, tempObjStart);
            if (tempObjEnd > tempObjStart) {
              const String tempJson = dayJson.substring(tempObjStart, tempObjEnd + 1);
              hasMax = parseFieldNumberFlexible(tempJson, "\"max\"", maxTemp);
              hasMin = parseFieldNumberFlexible(tempJson, "\"min\"", minTemp);
            }
          }
        }
        if (!hasMax) {
          parseFieldNumberFlexible(dayJson, "\"day\"", maxTemp);
        }
        if (!hasMin) {
          parseFieldNumberFlexible(dayJson, "\"night\"", minTemp);
        }

        int dayId = currentId;
        parseIntFrom(dayJson, "\"id\":", 0, dayId, nullptr);

        time_t localDt = static_cast<time_t>(dt + timezoneOffsetSec);
        tm localInfo{};
        gmtime_r(&localDt, &localInfo);
        parsedDaily[parsedDailyCount].dow = static_cast<uint8_t>(localInfo.tm_wday);
        parsedDaily[parsedDailyCount].high = static_cast<int16_t>(roundToInt(maxTemp));
        parsedDaily[parsedDailyCount].low = static_cast<int16_t>(roundToInt(minTemp));
        parsedDaily[parsedDailyCount].type = mapWeatherType(dayId);
        parsedDaily[parsedDailyCount].main[0] = '\0';
        parseStringFrom(
            dayJson, "\"main\":\"", 0, parsedDaily[parsedDailyCount].main,
            sizeof(parsedDaily[parsedDailyCount].main), nullptr);
        ++parsedDailyCount;
      }
    }
  }

  // Today card comes from daily[0] when available.
  if (parsedDailyCount > 0) {
    weather.todayHighF = parsedDaily[0].high;
    weather.todayLowF = parsedDaily[0].low;
  }

  if (parsedDailyCount < 5) {
    Serial.print("[OWM] Insufficient daily JSON entries. parsedDailyCount=");
    Serial.println(parsedDailyCount);
    return false;
  }

  // 4-day page should be tomorrow..+3 => daily[1..4].
  int dailyPageFilled = 0;
  const int startIndex = 1;
  for (int i = 0; i < 4; ++i) {
    const int srcIdx = startIndex + i;
    if (srcIdx >= parsedDailyCount) {
      break;
    }
    weather.dailyDow[i] = parsedDaily[srcIdx].dow;
    weather.dailyHighF[i] = parsedDaily[srcIdx].high;
    weather.dailyLowF[i] = parsedDaily[srcIdx].low;
    weather.dailyType[i] = parsedDaily[srcIdx].type;
    strncpy(weather.dailyMain[i], parsedDaily[srcIdx].main, sizeof(weather.dailyMain[i]) - 1);
    weather.dailyMain[i][sizeof(weather.dailyMain[i]) - 1] = '\0';
    ++dailyPageFilled;
  }
  if (dailyPageFilled < 4) {
    Serial.print("[OWM] Failed to populate 4-day rows from JSON. filled=");
    Serial.println(dailyPageFilled);
    return false;
  }

  weather.valid = true;
  Serial.print("[OWM] Weather success: tempF=");
  Serial.print(weather.temperatureF);
  Serial.print(" type=");
  Serial.print(static_cast<int>(weather.type));
  Serial.print(" rain=");
  Serial.print(weather.rainChancePct);
  Serial.print("% wind=");
  Serial.print(weather.windMph);
  Serial.print(" gust=");
  Serial.println(weather.gustMph);
  logParsedWeather(weather);
  return true;
}

// Runs full refresh flow: validate config, geocode ZIP, fetch weather payload.
bool OpenWeatherService::refreshWeather(WeatherData& weather, ProgressCallback progress) const {
  const char* zip = configService_.zipCode();
  const char* apiKey = configService_.apiKey();

  if (zip == nullptr || zip[0] == '\0' || apiKey == nullptr || apiKey[0] == '\0' || WiFi.status() != WL_CONNECTED) {
    Serial.print("[OWM] Missing config or WiFi down. zip='");
    Serial.print(zip == nullptr ? "(null)" : zip);
    Serial.print("' apiKeyLen=");
    Serial.print(apiKey == nullptr ? 0 : static_cast<int>(strlen(apiKey)));
    Serial.print(" wifi=");
    Serial.println(static_cast<int>(WiFi.status()));
    weather.valid = false;
    return false;
  }

  double lat = 0;
  double lon = 0;
  if (!fetchCoordinatesForZip(zip, apiKey, lat, lon, progress)) {
    weather.valid = false;
    return false;
  }

  if (!fetchWeatherByCoordinates(lat, lon, apiKey, weather, progress)) {
    weather.valid = false;
    return false;
  }

  return true;
}

// Returns last successfully resolved location label.
const char* OpenWeatherService::lastLocationName() const {
  return lastLocationName_;
}

// Returns API timezone offset (seconds east of UTC) from last successful parse.
int32_t OpenWeatherService::detectedUtcOffsetSeconds() const {
  return detectedUtcOffsetSeconds_;
}
