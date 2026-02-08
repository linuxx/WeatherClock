#include "WeatherIcons.h"

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

namespace {
void drawSun(Adafruit_GFX& d, int16_t x, int16_t y) {
  d.fillCircle(x + 12, y + 12, 6, SSD1306_WHITE);
  d.drawLine(x + 12, y + 2, x + 12, y + 0, SSD1306_WHITE);
  d.drawLine(x + 12, y + 24, x + 12, y + 22, SSD1306_WHITE);
  d.drawLine(x + 2, y + 12, x + 0, y + 12, SSD1306_WHITE);
  d.drawLine(x + 24, y + 12, x + 22, y + 12, SSD1306_WHITE);
}

void drawCloud(Adafruit_GFX& d, int16_t x, int16_t y) {
  d.fillRoundRect(x + 10, y + 12, 24, 10, 4, SSD1306_WHITE);
  d.fillCircle(x + 14, y + 12, 5, SSD1306_WHITE);
  d.fillCircle(x + 23, y + 9, 7, SSD1306_WHITE);
  d.fillCircle(x + 31, y + 12, 5, SSD1306_WHITE);
}

void drawRainDrops(Adafruit_GFX& d, int16_t x, int16_t y) {
  d.drawLine(x + 14, y + 24, x + 12, y + 28, SSD1306_WHITE);
  d.drawLine(x + 22, y + 24, x + 20, y + 28, SSD1306_WHITE);
  d.drawLine(x + 30, y + 24, x + 28, y + 28, SSD1306_WHITE);
}

void drawSnow(Adafruit_GFX& d, int16_t x, int16_t y) {
  d.drawCircle(x + 13, y + 26, 2, SSD1306_WHITE);
  d.drawCircle(x + 22, y + 28, 2, SSD1306_WHITE);
  d.drawCircle(x + 30, y + 26, 2, SSD1306_WHITE);
}

void drawLightning(Adafruit_GFX& d, int16_t x, int16_t y) {
  d.fillTriangle(x + 21, y + 22, x + 16, y + 30, x + 21, y + 30, SSD1306_WHITE);
  d.fillTriangle(x + 21, y + 30, x + 26, y + 24, x + 22, y + 24, SSD1306_WHITE);
}

void drawFogLines(Adafruit_GFX& d, int16_t x, int16_t y) {
  d.drawLine(x + 8, y + 24, x + 34, y + 24, SSD1306_WHITE);
  d.drawLine(x + 6, y + 28, x + 36, y + 28, SSD1306_WHITE);
}

void drawWind(Adafruit_GFX& d, int16_t x, int16_t y) {
  d.drawLine(x + 6, y + 10, x + 28, y + 10, SSD1306_WHITE);
  d.drawLine(x + 10, y + 16, x + 34, y + 16, SSD1306_WHITE);
  d.drawLine(x + 6, y + 22, x + 26, y + 22, SSD1306_WHITE);
}
}  // namespace

const char* weatherTypeLabel(WeatherType type) {
  switch (type) {
    case WeatherType::Clear:
      return "Clear";
    case WeatherType::PartlyCloudy:
      return "Partly Cloudy";
    case WeatherType::Cloudy:
      return "Cloudy";
    case WeatherType::Rain:
      return "Rain";
    case WeatherType::Thunderstorm:
      return "Thunderstorm";
    case WeatherType::Snow:
      return "Snow";
    case WeatherType::Fog:
      return "Fog";
    case WeatherType::Windy:
      return "Windy";
    case WeatherType::Count:
      return "Unknown";
  }
  return "Unknown";
}

void drawWeatherIcon(Adafruit_GFX& display, WeatherType type, int16_t x, int16_t y) {
  switch (type) {
    case WeatherType::Clear:
      drawSun(display, x + 8, y + 4);
      break;
    case WeatherType::PartlyCloudy:
      drawSun(display, x + 3, y + 0);
      drawCloud(display, x, y + 4);
      break;
    case WeatherType::Cloudy:
      drawCloud(display, x, y + 4);
      break;
    case WeatherType::Rain:
      drawCloud(display, x, y + 2);
      drawRainDrops(display, x, y);
      break;
    case WeatherType::Thunderstorm:
      drawCloud(display, x, y + 2);
      drawLightning(display, x, y);
      break;
    case WeatherType::Snow:
      drawCloud(display, x, y + 2);
      drawSnow(display, x, y);
      break;
    case WeatherType::Fog:
      drawCloud(display, x, y + 0);
      drawFogLines(display, x, y);
      break;
    case WeatherType::Windy:
      drawWind(display, x, y + 6);
      break;
    case WeatherType::Count:
      drawCloud(display, x, y + 4);
      break;
  }
}

