#pragma once

#include <stddef.h>
#include <LittleFS.h>
#include <WiFiManager.h>

/**
 * @brief Persists ZIP/API key config and exposes corresponding WiFiManager params.
 */
class OpenWeatherConfigService {
 public:
  /**
   * @brief Construct config service with empty defaults.
   */
  OpenWeatherConfigService();

  /**
   * @brief Load ZIP/API key from LittleFS.
   */
  void load();

  /**
   * @brief Register ZIP/API key fields into WiFiManager setup page.
   */
  void configurePortal(WiFiManager& manager);

  /**
   * @brief Apply values currently submitted in WiFiManager fields and persist them.
   */
  void applyFromConfig();

  /**
   * @brief Remove persisted ZIP/API key values.
   */
  void clearSaved();

  /**
   * @brief Sync in-memory values into WiFiManager parameter defaults.
   */
  void syncPortalValues();

  /**
   * @brief Get configured ZIP code string.
   */
  const char* zipCode() const;

  /**
   * @brief Get configured OpenWeather API key string.
   */
  const char* apiKey() const;

 private:
  static constexpr const char* kZipCodeFile = "/zipcode.txt";
  static constexpr const char* kApiKeyFile = "/openweather_api_key.txt";

  /**
   * @brief Ensure LittleFS is mounted.
   * @return True if filesystem is mounted and ready.
   */
  bool ensureFsMounted();

  /**
   * @brief Load a single line value from a filesystem file.
   * @return True if non-empty value was loaded.
   */
  bool loadFromFs(const char* filePath, char* output, size_t outputSize);

  /**
   * @brief Save a value to a filesystem file.
   */
  void saveToFs(const char* filePath, const char* value);

  bool fsMounted_ = false;
  char zipCodeValue_[16];
  char apiKeyValue_[65];
  WiFiManagerParameter zipCodeParam_;
  WiFiManagerParameter apiKeyParam_;
};
