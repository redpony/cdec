#ifndef TIMING_H
#define TIMING_H

#ifdef __CYGWIN__
# ifndef _POSIX_MONOTONIC_CLOCK
#  define _POSIX_MONOTONIC_CLOCK
# endif
#endif

#include <time.h>
#include <sys/time.h>
#include "clock_gettime_stub.c"

struct Timer {
  Timer() { Reset(); }
  void Reset()
  {
    clock_gettime(CLOCK_MONOTONIC, &start_t);
  }
  double Elapsed() const {
    timespec end_t;
    clock_gettime(CLOCK_MONOTONIC, &end_t);
    const double elapsed = (end_t.tv_sec - start_t.tv_sec)
                + (end_t.tv_nsec - start_t.tv_nsec) / 1000000000.0;
    return elapsed;
  }
 private:
  timespec start_t;
};

#endif
