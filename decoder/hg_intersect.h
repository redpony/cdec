#ifndef HG_INTERSECT_H_
#define HG_INTERSECT_H_

#include "lattice.h"

class Hypergraph;
namespace HG {
  bool Intersect(const Lattice& target, Hypergraph* hg);
};

#endif
