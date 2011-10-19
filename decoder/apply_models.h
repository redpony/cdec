#ifndef _APPLY_MODELS_H_
#define _APPLY_MODELS_H_

#include <iostream>

struct ModelSet;
struct Hypergraph;
struct SentenceMetadata;

struct exhaustive_t {};

struct IntersectionConfiguration {
enum {
  FULL,
  CUBE,
  FAST_CUBE_PRUNING,
  FAST_CUBE_PRUNING_2,
  N_ALGORITHMS
};

  const int algorithm; // 0 = full intersection, 1 = cube pruning
  const int pop_limit; // max number of pops off the heap at each node
  IntersectionConfiguration(int alg, int k) : algorithm(alg), pop_limit(k) {}
  IntersectionConfiguration(exhaustive_t /* t */) : algorithm(0), pop_limit() {}
};

inline std::ostream& operator<<(std::ostream& os, const IntersectionConfiguration& c) {
  if (c.algorithm == 0) { os << "FULL"; }
  else if (c.algorithm == 1) { os << "CUBE:k=" << c.pop_limit; }
  else if (c.algorithm == 2) { os << "FAST_CUBE_PRUNING"; }
  else if (c.algorithm == 3) { os << "FAST_CUBE_PRUNING_2"; }
  else if (c.algorithm == 4) { os << "N_ALGORITHMS"; }
  else os << "OTHER";
  return os;
}

void ApplyModelSet(const Hypergraph& in,
                   const SentenceMetadata& smeta,
                   const ModelSet& models,
                   const IntersectionConfiguration& config,
                   Hypergraph* out);

#endif
