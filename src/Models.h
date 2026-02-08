#pragma once

#include <stdint.h>
#include "WeatherIcons.h"

/**
 * @brief Current clock values rendered in the top band.
 */
struct ClockData {
  /** @brief Hour in 24-hour format (0-23). */
  uint8_t hour;
  /** @brief Minute (0-59). */
  uint8_t minute;
  /** @brief Month (1-12). */
  uint8_t month;
  /** @brief Day of month (1-31). */
  uint8_t day;
  /** @brief True when clock data was successfully refreshed. */
  bool valid;
};

/**
 * @brief Consolidated weather view model consumed by display pages.
 */
struct WeatherData {
  /** @brief Current ambient temperature in Fahrenheit. */
  int16_t temperatureF;
  /** @brief Current "feels like" temperature in Fahrenheit. */
  int16_t feelsLikeF;
  /** @brief Current precipitation probability in percent (0-100). */
  uint8_t rainChancePct;
  /** @brief Snow probability in percent (legacy; currently optional). */
  uint8_t snowChancePct;
  /** @brief Current summarized weather type. */
  WeatherType type;

  /** @brief Today's forecast high in Fahrenheit. */
  int16_t todayHighF;
  /** @brief Today's forecast low in Fahrenheit. */
  int16_t todayLowF;
  /** @brief Sunrise local hour (0-23). */
  uint8_t sunriseHour;
  /** @brief Sunrise local minute (0-59). */
  uint8_t sunriseMinute;
  /** @brief Sunset local hour (0-23). */
  uint8_t sunsetHour;
  /** @brief Sunset local minute (0-59). */
  uint8_t sunsetMinute;

  /** @brief Sustained wind speed in mph. */
  uint8_t windMph;
  /** @brief Wind gust speed in mph. */
  uint8_t gustMph;
  /** @brief Wind direction in degrees from north (0-359). */
  uint16_t windDeg;
  /** @brief Advisory text line for advisories page. */
  char advisory[32];

  /** @brief Hour labels (24-hour values) for 4 hourly rows. */
  uint8_t hourlyHour24[4];
  /** @brief Hourly temperatures in Fahrenheit for 4 rows. */
  int16_t hourlyTempF[4];
  /** @brief Hourly icon types for 4 rows. */
  WeatherType hourlyType[4];
  /** @brief Hourly condition text ("Clear", "Clouds", etc.) for 4 rows. */
  char hourlyMain[4][12];

  /** @brief Day-of-week indices for 4 daily rows (0=Sun..6=Sat). */
  uint8_t dailyDow[4];
  /** @brief Daily high temperatures in Fahrenheit for 4 rows. */
  int16_t dailyHighF[4];
  /** @brief Daily low temperatures in Fahrenheit for 4 rows. */
  int16_t dailyLowF[4];
  /** @brief Daily icon types for 4 rows. */
  WeatherType dailyType[4];
  /** @brief Daily condition text ("Rain", "Clouds", etc.) for 4 rows. */
  char dailyMain[4][12];

  /** @brief True when weather payload was successfully parsed. */
  bool valid;
};
