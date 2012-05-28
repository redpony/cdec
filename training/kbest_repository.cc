#include "kbest_repository.h"

#include <boost/functional/hash.hpp>

using namespace std;

struct ApproxVectorHasher {
  static const size_t MASK = 0xFFFFFFFFull;
  union UType {
    double f;   // leave as double
    size_t i;
  };
  static inline double round(const double x) {
    UType t;
    t.f = x;
    size_t r = t.i & MASK;
    if ((r << 1) > MASK)
      t.i += MASK - r + 1;
    else
      t.i &= (1ull - MASK);
    return t.f;
  }
  size_t operator()(const SparseVector<double>& x) const {
    size_t h = 0x573915839;
    for (SparseVector<double>::const_iterator it = x.begin(); it != x.end(); ++it) {
      UType t;
      t.f = it->second;
      if (t.f) {
        size_t z = (t.i >> 32);
        boost::hash_combine(h, it->first);
        boost::hash_combine(h, z);
      }
    }
    return h;
  }
};

