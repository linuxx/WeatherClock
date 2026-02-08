#include "OpenWeatherConfigService.h"

#include <string.h>

OpenWeatherConfigService::OpenWeatherConfigService()
    : zipCodeValue_{0},
      apiKeyValue_{0},
      zipCodeParam_("zip", "ZIP Code", zipCodeValue_, sizeof(zipCodeValue_)),
      apiKeyParam_("owm_key", "OpenWeather API Key", apiKeyValue_, sizeof(apiKeyValue_)) {
  zipCodeValue_[0] = '\0';
  apiKeyValue_[0] = '\0';
}

bool OpenWeatherConfigService::ensureFsMounted() {
  if (fsMounted_) {
    return true;
  }
  fsMounted_ = LittleFS.begin();
  return fsMounted_;
}

bool OpenWeatherConfigService::loadFromFs(const char* filePath, char* output, size_t outputSize) {
  if (!ensureFsMounted() || filePath == nullptr || output == nullptr || outputSize == 0 || !LittleFS.exists(filePath)) {
    return false;
  }

  File file = LittleFS.open(filePath, "r");
  if (!file) {
    return false;
  }

  String value = file.readStringUntil('\n');
  value.trim();
  strncpy(output, value.c_str(), outputSize - 1);
  output[outputSize - 1] = '\0';
  return output[0] != '\0';
}

void OpenWeatherConfigService::saveToFs(const char* filePath, const char* value) {
  if (!ensureFsMounted() || filePath == nullptr || value == nullptr) {
    return;
  }

  File file = LittleFS.open(filePath, "w");
  if (!file) {
    return;
  }
  file.print(value);
}

void OpenWeatherConfigService::load() {
  if (!loadFromFs(kZipCodeFile, zipCodeValue_, sizeof(zipCodeValue_))) {
    zipCodeValue_[0] = '\0';
  }
  if (!loadFromFs(kApiKeyFile, apiKeyValue_, sizeof(apiKeyValue_))) {
    apiKeyValue_[0] = '\0';
  }
  syncPortalValues();
}

void OpenWeatherConfigService::configurePortal(WiFiManager& manager) {
  syncPortalValues();
  manager.addParameter(&zipCodeParam_);
  manager.addParameter(&apiKeyParam_);
}

void OpenWeatherConfigService::applyFromConfig() {
  const char* zipParam = zipCodeParam_.getValue();
  if (zipParam != nullptr && zipParam[0] != '\0') {
    strncpy(zipCodeValue_, zipParam, sizeof(zipCodeValue_) - 1);
    zipCodeValue_[sizeof(zipCodeValue_) - 1] = '\0';
  }

  const char* apiKeyParam = apiKeyParam_.getValue();
  if (apiKeyParam != nullptr && apiKeyParam[0] != '\0') {
    strncpy(apiKeyValue_, apiKeyParam, sizeof(apiKeyValue_) - 1);
    apiKeyValue_[sizeof(apiKeyValue_) - 1] = '\0';
  }

  saveToFs(kZipCodeFile, zipCodeValue_);
  saveToFs(kApiKeyFile, apiKeyValue_);
  syncPortalValues();
}

void OpenWeatherConfigService::clearSaved() {
  zipCodeValue_[0] = '\0';
  apiKeyValue_[0] = '\0';
  if (!ensureFsMounted()) {
    return;
  }

  if (LittleFS.exists(kZipCodeFile)) {
    LittleFS.remove(kZipCodeFile);
  }
  if (LittleFS.exists(kApiKeyFile)) {
    LittleFS.remove(kApiKeyFile);
  }
}

const char* OpenWeatherConfigService::zipCode() const {
  return zipCodeValue_;
}

const char* OpenWeatherConfigService::apiKey() const {
  return apiKeyValue_;
}

void OpenWeatherConfigService::syncPortalValues() {
  zipCodeParam_.setValue(zipCodeValue_, static_cast<int>(sizeof(zipCodeValue_)));
  apiKeyParam_.setValue(apiKeyValue_, static_cast<int>(sizeof(apiKeyValue_)));
}
