#ifndef _LAZY_H_
#define _LAZY_H_

#include "weights.h"
#include <vector>

class Hypergraph;

void PassToLazy(const char *model_file, const std::vector<weight_t> &weights, unsigned int pop_limit, const Hypergraph &hg);

#endif // _LAZY_H_
