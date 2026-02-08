#pragma once

#include <stdint.h>
#include "WeatherIcons.h"

struct ClockData {
  uint8_t hour;
  uint8_t minute;
  uint8_t month;
  uint8_t day;
  bool valid;
};

struct WeatherData {
  int16_t temperatureF;
  int16_t feelsLikeF;
  uint8_t rainChancePct;
  uint8_t snowChancePct;
  WeatherType type;
  int16_t todayHighF;
  int16_t todayLowF;
  uint8_t sunriseHour;
  uint8_t sunriseMinute;
  uint8_t sunsetHour;
  uint8_t sunsetMinute;
  uint8_t windMph;
  uint8_t gustMph;
  uint16_t windDeg;
  char advisory[32];
  uint8_t hourlyHour24[4];
  int16_t hourlyTempF[4];
  WeatherType hourlyType[4];
  char hourlyMain[4][12];
  uint8_t dailyDow[4];  // 0=Sun..6=Sat
  int16_t dailyHighF[4];
  int16_t dailyLowF[4];
  WeatherType dailyType[4];
  char dailyMain[4][12];
  bool valid;
};
