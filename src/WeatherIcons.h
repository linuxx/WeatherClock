#pragma once

#include <stdint.h>
#include <Adafruit_GFX.h>

/**
 * @brief Weather categories shared by parser and display renderer.
 */
enum class WeatherType : uint8_t {
  Clear,
  PartlyCloudy,
  Cloudy,
  Rain,
  Thunderstorm,
  Snow,
  Fog,
  Windy,
  Count
};

/**
 * @brief Convert WeatherType to human-readable label for text pages.
 * @param type Weather category.
 * @return Short label string.
 */
const char* weatherTypeLabel(WeatherType type);

/**
 * @brief Draw 40x32-ish monochrome weather icon.
 * @param display Destination Adafruit_GFX display.
 * @param type Weather category.
 * @param x Left position.
 * @param y Top position.
 */
void drawWeatherIcon(Adafruit_GFX& display, WeatherType type, int16_t x, int16_t y);
