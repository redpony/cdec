#ifndef UTILS__BEST_H
#define UTILS__BEST_H

#include "max_plus.h"

typedef MaxPlus<double> best_t;

inline bool operator <(best_t const& a,best_t const& b) {
  return a.v_>b.v_; // intentionally reversed, so default min-heap, sort, etc. put best first.
}


#endif
