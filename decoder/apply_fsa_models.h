#ifndef _APPLY_FSA_MODELS_H_
#define _APPLY_FSA_MODELS_H_

#include <string>
#include <iostream>
#include "feature_vector.h"
#include "named_enum.h"

struct FsaFeatureFunction;
struct Hypergraph;
struct SentenceMetadata;
struct HgCFG;


#define FSA_BY(X,t)                              \
  X(t,BU_CUBE,)                                 \
  X(t,BU_FULL,)                                 \
  X(t,EARLEY,)                                  \

#define FSA_BY_TYPE FsaBy

DECLARE_NAMED_ENUM(FSA_BY)

struct ApplyFsaBy {
/*enum {
  BU_CUBE,
  BU_FULL,
  EARLEY,
  N_ALGORITHMS
  };*/
  int pop_limit; // only applies to BU_FULL so far
  bool IsBottomUp() const {
    return algorithm==BU_FULL || algorithm==BU_CUBE;
  }
  int BottomUpAlgorithm() const;
  FsaBy algorithm;
  std::string name() const;
  friend inline std::ostream &operator << (std::ostream &o,ApplyFsaBy const& c) {
    o << c.name();
    if (c.algorithm==BU_CUBE)
      o << "("<<c.pop_limit<<")";
    return o;
  }
  explicit ApplyFsaBy(FsaBy alg, int poplimit=200);
  ApplyFsaBy(std::string const& name, int poplimit=200);
  ApplyFsaBy(const ApplyFsaBy &o) : algorithm(o.algorithm) {  }
  static std::string all_names(); // space separated
};

void ApplyFsaModels(HgCFG &hg_or_cfg_in,
                    const SentenceMetadata& smeta,
                    const FsaFeatureFunction& fsa,
                    DenseWeightVector const& weights, // pre: in is weighted by these (except with fsa featval=0 before this)
                    ApplyFsaBy const& cfg,
                    Hypergraph* out);

void ApplyFsaModels(Hypergraph const& ih,
                    const SentenceMetadata& smeta,
                    const FsaFeatureFunction& fsa,
                    DenseWeightVector const& weights, // pre: in is weighted by these (except with fsa featval=0 before this)
                    ApplyFsaBy const& cfg,
                    Hypergraph* out);


#endif
