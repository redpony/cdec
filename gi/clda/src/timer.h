#ifndef _TIMER_STATS_H_
#define _TIMER_STATS_H_

#include <ctime>

struct Timer {
  Timer() { Reset(); }
  void Reset() {
    start_t = clock();
  }
  double Elapsed() const {
    const clock_t end_t = clock();
    const double elapsed = (end_t - start_t) / 1000000.0;
    return elapsed;
  }
 private:
  std::clock_t start_t;
};

#endif
