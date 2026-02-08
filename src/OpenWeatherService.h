#pragma once

#include <Arduino.h>
#include "Models.h"
#include "OpenWeatherConfigService.h"

class OpenWeatherService {
 public:
  using ProgressCallback = void (*)(const char* title, const String& line1, const String& line2, const String& line3);

  explicit OpenWeatherService(const OpenWeatherConfigService& configService);
  bool refreshWeather(WeatherData& weather, ProgressCallback progress = nullptr) const;
  const char* lastLocationName() const;
  int32_t detectedUtcOffsetSeconds() const;

 private:
  static bool parseStringFrom(
      const String& json, const char* key, int start, char* outBuf, size_t outBufSize, int* valuePos = nullptr);
  bool fetchCoordinatesForZip(
      const char* zipInput, const char* apiKey, double& lat, double& lon, ProgressCallback progress) const;
  bool fetchWeatherByCoordinates(
      double lat, double lon, const char* apiKey, WeatherData& weather, ProgressCallback progress) const;
  static bool parseNumberFrom(const String& json, const char* key, int start, double& outValue, int* valuePos = nullptr);
  static bool parseIntFrom(const String& json, const char* key, int start, int& outValue, int* valuePos = nullptr);
  static int findSection(const String& json, const char* sectionKey);
  static int roundToInt(double value);
  static WeatherType mapWeatherType(int weatherId);
  static bool parseNumber(const String& json, const char* key, double& outValue);
  static bool parseInt(const String& json, const char* key, int& outValue);

  const OpenWeatherConfigService& configService_;
  mutable char lastLocationName_[40] = {0};
  mutable int32_t detectedUtcOffsetSeconds_ = 0;
};
