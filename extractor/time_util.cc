#include "time_util.h"

double GetDuration(const Clock::time_point& start_time,
                   const Clock::time_point& stop_time) {
  return duration_cast<milliseconds>(stop_time - start_time).count() / 1000.0;
}
