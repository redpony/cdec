#ifndef _INCREMENTAL_H_
#define _INCREMENTAL_H_

#include "weights.h"
#include <vector>

class Hypergraph;

void PassToIncremental(const char *model_file, const std::vector<weight_t> &weights, unsigned int pop_limit, const Hypergraph &hg);

#endif // _INCREMENTAL_H_
