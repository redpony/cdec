#ifndef _TIME_UTIL_H_
#define _TIME_UTIL_H_

#include <chrono>

using namespace std;
using namespace chrono;

namespace extractor {

typedef high_resolution_clock Clock;

// Computes the duration in seconds of the specified time interval.
double GetDuration(const Clock::time_point& start_time,
                   const Clock::time_point& stop_time);

} // namespace extractor

#endif
