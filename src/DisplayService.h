#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "Models.h"

/**
 * @brief Encapsulates all OLED drawing/layout logic.
 *
 * This class owns no weather/time state; it only renders values passed in by callers.
 */
class DisplayService {
 public:
  /**
   * @brief Construct a display renderer.
   * @param display Reference to initialized SSD1306 display object.
   */
  explicit DisplayService(Adafruit_SSD1306& display);

  /**
   * @brief Enable/disable network activity icon animation.
   * @param active True to show network icon.
   * @param frame Current animation frame index.
   */
  void setNetworkActivity(bool active, uint8_t frame);

  /**
   * @brief Set local IP text used on error screens.
   * @param ip Local IP string (e.g. 192.168.1.42).
   */
  void setLocalIp(const String& ip);

  /**
   * @brief Draw the boot splash screen.
   */
  void drawBootScreen();

  /**
   * @brief Draw a generic status/info screen with up to 3 lines.
   * @param title Top title row.
   * @param line1 First body line.
   * @param line2 Second body line.
   * @param line3 Third body line.
   */
  void drawStatusScreen(const char* title, const String& line1, const String& line2 = "", const String& line3 = "");

  /**
   * @brief Draw the home layout (clock top + weather bottom).
   * @param clock Clock values.
   * @param weather Weather values.
   * @param showColon Whether to show blinking colon in time.
   */
  void drawLayoutFrame(const ClockData& clock, const WeatherData& weather, bool showColon);

  /**
   * @brief Draw one of the UI pages.
   * @param pageIndex 0=home, 1..N detail pages.
   * @param clock Clock values.
   * @param weather Weather values.
   * @param showColon Whether to show blinking colon in time.
   */
  void drawPage(uint8_t pageIndex, const ClockData& clock, const WeatherData& weather, bool showColon);

 private:
  static constexpr uint8_t kScreenWidth = 128;
  static constexpr uint8_t kTopBandHeight = 16;

  /**
   * @brief Return compact weather label fallback.
   */
  static const char* shortWeatherLabel(WeatherType type);

  /**
   * @brief Convert degrees to 8-point cardinal text (N, NE, E, ...).
   */
  static const char* windDirectionLabel(uint16_t deg);

  /**
   * @brief Convert weekday index (0=Sun..6=Sat) to abbreviated label.
   */
  static const char* shortDayName(uint8_t dow);

  /**
   * @brief Format 24-hour value as compact 12-hour label (e.g. 2p, 11a).
   */
  static String formatHourLabel(uint8_t hour24);

  /**
   * @brief Draw animated network glyph.
   */
  void drawNetworkActivityIcon(int16_t x, int16_t y) const;

  /**
   * @brief Draw top clock/date band.
   */
  void drawTopBand(const ClockData& clock, bool showColon);

  /**
   * @brief Draw home-page weather section.
   */
  void drawBottomBand(const WeatherData& weather);

  /**
   * @brief Draw "Today" detail page.
   */
  void drawTodayPage(const WeatherData& weather);

  /**
   * @brief Draw "Hourly" detail page.
   */
  void drawHourlyPage(const ClockData& clock, const WeatherData& weather);

  /**
   * @brief Draw "4-Day" detail page.
   */
  void drawFourDayPage(const WeatherData& weather);

  /**
   * @brief Draw advisories detail page.
   */
  void drawAdvisoriesPage(const WeatherData& weather);

  /**
   * @brief Draw wind detail page.
   */
  void drawWindPage(const WeatherData& weather);

  Adafruit_SSD1306& display_;
  bool networkBusy_ = false;
  uint8_t networkAnimFrame_ = 0;
  String localIp_;
};
