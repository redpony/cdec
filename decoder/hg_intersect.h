#ifndef _HG_INTERSECT_H_
#define _HG_INTERSECT_H_

#include "lattice.h"

class Hypergraph;
namespace HG {
  bool Intersect(const Lattice& target, Hypergraph* hg);
};

#endif
