#include "OpenWeatherConfigService.h"

#include <Arduino.h>
#include <string.h>

/**
 * Construct persistent ZIP/API-key config service and bind parameter buffers.
 */
OpenWeatherConfigService::OpenWeatherConfigService()
    : zipCodeValue_{0},
      apiKeyValue_{0},
      zipCodeParam_("zip", "ZIP Code", zipCodeValue_, sizeof(zipCodeValue_)),
      apiKeyParam_("owm_key", "OpenWeather API Key", apiKeyValue_, sizeof(apiKeyValue_)) {
  zipCodeValue_[0] = '\0';
  apiKeyValue_[0] = '\0';
}

/**
 * Ensure LittleFS is mounted once before file operations.
 */
bool OpenWeatherConfigService::ensureFsMounted() {
  if (fsMounted_) {
    return true;
  }
  // Format-on-fail prevents "works until reboot" behavior on uninitialized FS.
  fsMounted_ = LittleFS.begin(true);
  if (!fsMounted_) {
    Serial.println("[CFG] LittleFS mount failed");
  }
  return fsMounted_;
}

/**
 * Load a trimmed text value from LittleFS into fixed-size output buffer.
 */
bool OpenWeatherConfigService::loadFromFs(const char* filePath, char* output, size_t outputSize) {
  if (!ensureFsMounted() || filePath == nullptr || output == nullptr || outputSize == 0 || !LittleFS.exists(filePath)) {
    return false;
  }

  File file = LittleFS.open(filePath, "r");
  if (!file) {
    return false;
  }

  String value = file.readStringUntil('\n');
  file.close();
  value.trim();
  strncpy(output, value.c_str(), outputSize - 1);
  output[outputSize - 1] = '\0';
  return output[0] != '\0';
}

/**
 * Persist a text value to LittleFS.
 */
void OpenWeatherConfigService::saveToFs(const char* filePath, const char* value) {
  if (!ensureFsMounted() || filePath == nullptr || value == nullptr) {
    return;
  }

  File file = LittleFS.open(filePath, "w");
  if (!file) {
    Serial.print("[CFG] Failed to open file for write: ");
    Serial.println(filePath);
    return;
  }
  file.print(value);
  file.flush();
  file.close();
}

/**
 * Load ZIP/API-key settings from filesystem and mirror into portal fields.
 */
void OpenWeatherConfigService::load() {
  if (!loadFromFs(kZipCodeFile, zipCodeValue_, sizeof(zipCodeValue_))) {
    zipCodeValue_[0] = '\0';
  }
  if (!loadFromFs(kApiKeyFile, apiKeyValue_, sizeof(apiKeyValue_))) {
    apiKeyValue_[0] = '\0';
  }
  syncPortalValues();
}

/**
 * Register ZIP/API-key parameters in WiFiManager setup page.
 */
void OpenWeatherConfigService::configurePortal(WiFiManager& manager) {
  // Ensure current values are shown when user opens /param page.
  syncPortalValues();
  manager.addParameter(&zipCodeParam_);
  manager.addParameter(&apiKeyParam_);
}

/**
 * Apply submitted WiFiManager values into memory and persist them.
 */
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

/**
 * Clear in-memory and persisted ZIP/API-key settings.
 */
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

/**
 * Return configured ZIP code.
 */
const char* OpenWeatherConfigService::zipCode() const {
  return zipCodeValue_;
}

/**
 * Return configured OpenWeather API key.
 */
const char* OpenWeatherConfigService::apiKey() const {
  return apiKeyValue_;
}

/**
 * Push current in-memory values into WiFiManager field defaults.
 */
void OpenWeatherConfigService::syncPortalValues() {
  // Push current values into WiFiManager parameter defaults.
  zipCodeParam_.setValue(zipCodeValue_, static_cast<int>(sizeof(zipCodeValue_)));
  apiKeyParam_.setValue(apiKeyValue_, static_cast<int>(sizeof(apiKeyValue_)));
}
