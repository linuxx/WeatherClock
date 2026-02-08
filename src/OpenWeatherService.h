#pragma once

#include <Arduino.h>
#include "Models.h"
#include "OpenWeatherConfigService.h"

/**
 * @brief Fetches and parses OpenWeather geocode + OneCall payloads into WeatherData.
 */
class OpenWeatherService {
 public:
  /**
   * @brief Optional progress callback for status screens/logging.
   * @param title Status title.
   * @param line1 First status line.
   * @param line2 Second status line.
   * @param line3 Third status line.
   */
  using ProgressCallback = void (*)(const char* title, const String& line1, const String& line2, const String& line3);

  /**
   * @brief Construct service with a config provider.
   * @param configService ZIP/API-key storage + portal integration service.
   */
  explicit OpenWeatherService(const OpenWeatherConfigService& configService);

  /**
   * @brief Refresh weather using configured ZIP + API key.
   * @param weather Output model filled on success.
   * @param progress Optional callback for staged status updates.
   * @return True if weather was fetched and parsed successfully.
   */
  bool refreshWeather(WeatherData& weather, ProgressCallback progress = nullptr) const;

  /**
   * @brief Return last successfully resolved location name.
   */
  const char* lastLocationName() const;

  /**
   * @brief Return timezone offset from API payload (seconds east of UTC).
   */
  int32_t detectedUtcOffsetSeconds() const;

 private:
  /**
   * @brief Parse a string value by key starting at a given offset.
   */
  static bool parseStringFrom(
      const String& json, const char* key, int start, char* outBuf, size_t outBufSize, int* valuePos = nullptr);

  /**
   * @brief Resolve ZIP to latitude/longitude via geocoding endpoint.
   */
  bool fetchCoordinatesForZip(
      const char* zipInput, const char* apiKey, double& lat, double& lon, ProgressCallback progress) const;

  /**
   * @brief Fetch and parse OneCall weather payload for given coordinates.
   */
  bool fetchWeatherByCoordinates(
      double lat, double lon, const char* apiKey, WeatherData& weather, ProgressCallback progress) const;

  /**
   * @brief Parse floating-point value by key starting at an offset.
   */
  static bool parseNumberFrom(const String& json, const char* key, int start, double& outValue, int* valuePos = nullptr);

  /**
   * @brief Parse integer value by key starting at an offset.
   */
  static bool parseIntFrom(const String& json, const char* key, int start, int& outValue, int* valuePos = nullptr);

  /**
   * @brief Find the beginning of an object/array section by key.
   */
  static int findSection(const String& json, const char* sectionKey);

  /**
   * @brief Round floating-point value to nearest integer.
   */
  static int roundToInt(double value);

  /**
   * @brief Map OpenWeather condition id to local WeatherType.
   */
  static WeatherType mapWeatherType(int weatherId);

  /**
   * @brief Parse floating-point value by key from root.
   */
  static bool parseNumber(const String& json, const char* key, double& outValue);

  /**
   * @brief Parse integer value by key from root.
   */
  static bool parseInt(const String& json, const char* key, int& outValue);

  const OpenWeatherConfigService& configService_;
  mutable char lastLocationName_[40] = {0};
  mutable int32_t detectedUtcOffsetSeconds_ = 0;
};
