#ifndef _LAZY_H_
#define _LAZY_H_

#include "weights.h"
#include <vector>

class Hypergraph;

void PassToLazy(const Hypergraph &hg, const std::vector<weight_t> &weights);

#endif // _LAZY_H_
