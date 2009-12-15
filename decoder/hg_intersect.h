#ifndef _HG_INTERSECT_H_
#define _HG_INTERSECT_H_

#include <vector>

#include "lattice.h"

class Hypergraph;
struct HG {
  static bool Intersect(const Lattice& target, Hypergraph* hg);
};

#endif
