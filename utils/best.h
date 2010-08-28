#ifndef UTILS__BEST_H
#define UTILS__BEST_H

#include "max_plus.h"

typedef MaxPlus<double> best_t;

inline bool better(best_t const& a,best_t const& b) {
  return a.v_>b.v_; // intentionally reversed, so default min-heap, sort, etc. put best first.
}

inline bool operator <(best_t const& a,best_t const& b) {
  return a.v_>b.v_; // intentionally reversed, so default min-heap, sort, etc. put best first.
}
struct BetterP {
  inline bool operator ()(best_t const& a,best_t const& b) const {
    return a.v_>b.v_; // intentionally reversed, so default min-heap, sort, etc. put best first.
  }
};

inline void maybe_improve(best_t &a,best_t const& b) {
  if (a.v_>b.v_)
    a.v_=b.v_;
}

template <class O>
inline void maybe_improve(best_t &a,O const& b) {
  if (a.v_>b.v_)
    a.v_=b.v_;
}

#endif
