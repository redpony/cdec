#include "time_util.h"

namespace extractor {

double GetDuration(const Clock::time_point& start_time,
                   const Clock::time_point& stop_time) {
  return duration_cast<milliseconds>(stop_time - start_time).count() / 1000.0;
}

} // namespace extractor
