#pragma once

#include <stdint.h>
#include <Adafruit_GFX.h>

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

const char* weatherTypeLabel(WeatherType type);
void drawWeatherIcon(Adafruit_GFX& display, WeatherType type, int16_t x, int16_t y);

