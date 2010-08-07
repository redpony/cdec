#ifndef _APPLY_FSA_MODELS_H_
#define _APPLY_FSA_MODELS_H_

#include <iostream>
#include "feature_vector.h"

struct FsaFeatureFunction;
struct Hypergraph;
struct SentenceMetadata;

struct ApplyFsaBy {
  enum {
    BU_CUBE,
    BU_FULL,
    EARLEY,
    N_ALGORITHMS
  };
  int pop_limit; // only applies to BU_FULL so far
  bool IsBottomUp() const {
    return algorithm==BU_FULL || algorithm==BU_CUBE;
  }
  int BottomUpAlgorithm() const;
  int algorithm;
  std::string name() const;
  friend inline std::ostream &operator << (std::ostream &o,ApplyFsaBy const& c) {
    return o << c.name();
  }
  explicit ApplyFsaBy(int alg, int poplimit=200);
  ApplyFsaBy(std::string const& name, int poplimit=200);
  ApplyFsaBy(const ApplyFsaBy &o) : algorithm(o.algorithm) {  }
  static std::string all_names(); // space separated
};


void ApplyFsaModels(const Hypergraph& in,
                    const SentenceMetadata& smeta,
                    const FsaFeatureFunction& fsa,
                    DenseWeightVector const& weights, // pre: in is weighted by these (except with fsa featval=0 before this)
                    ApplyFsaBy const& cfg,
                    Hypergraph* out);

#endif
