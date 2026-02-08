#pragma once

#include <stddef.h>
#include <LittleFS.h>
#include <WiFiManager.h>

class OpenWeatherConfigService {
 public:
  OpenWeatherConfigService();

  void load();
  void configurePortal(WiFiManager& manager);
  void applyFromConfig();
  void clearSaved();
  void syncPortalValues();
  const char* zipCode() const;
  const char* apiKey() const;

 private:
  static constexpr const char* kZipCodeFile = "/zipcode.txt";
  static constexpr const char* kApiKeyFile = "/openweather_api_key.txt";

  bool ensureFsMounted();
  bool loadFromFs(const char* filePath, char* output, size_t outputSize);
  void saveToFs(const char* filePath, const char* value);

  bool fsMounted_ = false;
  char zipCodeValue_[16];
  char apiKeyValue_[65];
  WiFiManagerParameter zipCodeParam_;
  WiFiManagerParameter apiKeyParam_;
};
