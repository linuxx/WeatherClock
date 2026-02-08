#pragma once

#include "Models.h"

class TimeService {
 public:
  TimeService();
  void setUtcOffsetSeconds(int32_t offsetSeconds);
  int32_t utcOffsetSeconds() const;
  bool syncFromNtp();
  bool refreshClockData(ClockData& clock) const;

 private:
  int32_t utcOffsetSeconds_ = 0;
};
