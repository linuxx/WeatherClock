#include "TimeService.h"

#include <Arduino.h>
#include <time.h>

TimeService::TimeService() : utcOffsetSeconds_(0) {}

void TimeService::setUtcOffsetSeconds(int32_t offsetSeconds) {
  utcOffsetSeconds_ = offsetSeconds;
  Serial.print("[TIME] UTC offset set to seconds: ");
  Serial.println(utcOffsetSeconds_);
}

int32_t TimeService::utcOffsetSeconds() const {
  return utcOffsetSeconds_;
}

bool TimeService::syncFromNtp() {
  Serial.println("[TIME] Starting NTP sync (attempt 1)");
  configTime(0, 0, "0.us.pool.ntp.org", "1.us.pool.ntp.org", "2.us.pool.ntp.org");
  const unsigned long firstAttemptStart = millis();
  while (time(nullptr) < 8 * 3600 * 2 && (millis() - firstAttemptStart) < 12000) {
    delay(100);
  }
  if (time(nullptr) >= 8 * 3600 * 2) {
    Serial.println("[TIME] NTP sync succeeded on attempt 1");
    return true;
  }

  Serial.println("[TIME] NTP sync attempt 1 failed, trying attempt 2");
  configTime(0, 0, "1.us.pool.ntp.org", "2.us.pool.ntp.org", "3.us.pool.ntp.org");
  const unsigned long secondAttemptStart = millis();
  while (time(nullptr) < 8 * 3600 * 2 && (millis() - secondAttemptStart) < 12000) {
    delay(100);
  }

  const bool ok = time(nullptr) >= 8 * 3600 * 2;
  Serial.print("[TIME] NTP sync attempt 2 result: ");
  Serial.println(ok ? "success" : "failed");
  return ok;
}

bool TimeService::refreshClockData(ClockData& clock) const {
  const time_t utcNow = time(nullptr);
  if (utcNow < 8 * 3600 * 2) {
    Serial.println("[TIME] refreshClockData failed: system time not valid yet");
    clock.valid = false;
    return false;
  }

  const time_t localNow = utcNow + utcOffsetSeconds_;
  tm timeInfo{};
  gmtime_r(&localNow, &timeInfo);
  clock.hour = static_cast<uint8_t>(timeInfo.tm_hour);
  clock.minute = static_cast<uint8_t>(timeInfo.tm_min);
  clock.month = static_cast<uint8_t>(timeInfo.tm_mon + 1);
  clock.day = static_cast<uint8_t>(timeInfo.tm_mday);
  clock.valid = true;
  return true;
}
