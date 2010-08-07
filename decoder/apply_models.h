#ifndef _APPLY_MODELS_H_
#define _APPLY_MODELS_H_

struct ModelSet;
struct Hypergraph;
struct SentenceMetadata;

struct exhaustive_t {};

struct IntersectionConfiguration {
enum {
  FULL,
  CUBE,
  N_ALGORITHMS
};

  const int algorithm; // 0 = full intersection, 1 = cube pruning
  const int pop_limit; // max number of pops off the heap at each node
  IntersectionConfiguration(int alg, int k) : algorithm(alg), pop_limit(k) {}
  IntersectionConfiguration(exhaustive_t /* t */) : algorithm(0), pop_limit() {}
};

void ApplyModelSet(const Hypergraph& in,
                   const SentenceMetadata& smeta,
                   const ModelSet& models,
                   const IntersectionConfiguration& config,
                   Hypergraph* out);

#endif
