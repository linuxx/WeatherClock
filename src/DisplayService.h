#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "Models.h"

class DisplayService {
 public:
  explicit DisplayService(Adafruit_SSD1306& display);

  void setNetworkActivity(bool active, uint8_t frame);
  void drawBootScreen();
  void drawStatusScreen(const char* title, const String& line1, const String& line2 = "", const String& line3 = "");
  void drawLayoutFrame(const ClockData& clock, const WeatherData& weather, bool showColon);
  void drawPage(uint8_t pageIndex, const ClockData& clock, const WeatherData& weather, bool showColon);

 private:
  static constexpr uint8_t kScreenWidth = 128;
  static constexpr uint8_t kTopBandHeight = 16;

  static const char* shortWeatherLabel(WeatherType type);
  static const char* windDirectionLabel(uint16_t deg);
  static const char* shortDayName(uint8_t dow);
  static String formatHourLabel(uint8_t hour24);
  void drawNetworkActivityIcon(int16_t x, int16_t y) const;
  void drawTopBand(const ClockData& clock, bool showColon);
  void drawBottomBand(const WeatherData& weather);
  void drawTodayPage(const WeatherData& weather);
  void drawHourlyPage(const WeatherData& weather);
  void drawFourDayPage(const WeatherData& weather);
  void drawAdvisoriesPage(const WeatherData& weather);
  void drawWindPage(const WeatherData& weather);

  Adafruit_SSD1306& display_;
  bool networkBusy_ = false;
  uint8_t networkAnimFrame_ = 0;
};
