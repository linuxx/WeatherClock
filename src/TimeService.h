#pragma once

#include "Models.h"

/**
 * @brief Handles NTP sync and UTC-offset-based local time conversion.
 */
class TimeService {
 public:
  /**
   * @brief Construct time service with zero UTC offset.
   */
  TimeService();

  /**
   * @brief Set local offset from UTC in seconds.
   * @param offsetSeconds Offset returned by weather API timezone offset.
   */
  void setUtcOffsetSeconds(int32_t offsetSeconds);

  /**
   * @brief Get configured local offset from UTC in seconds.
   */
  int32_t utcOffsetSeconds() const;

  /**
   * @brief Sync system UTC clock from NTP servers.
   * @return True if UTC time was synchronized successfully.
   */
  bool syncFromNtp();

  /**
   * @brief Convert current UTC epoch to local clock fields.
   * @param clock Output structure to populate.
   * @return True if clock fields are valid.
   */
  bool refreshClockData(ClockData& clock) const;

 private:
  int32_t utcOffsetSeconds_ = 0;
};
