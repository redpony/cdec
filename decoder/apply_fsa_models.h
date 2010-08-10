#ifndef _APPLY_FSA_MODELS_H_
#define _APPLY_FSA_MODELS_H_

#include <string>
#include <iostream>
#include "feature_vector.h"
#include "cfg.h"

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
    o << c.name();
    if (c.algorithm==BU_CUBE)
      o << "("<<c.pop_limit<<")";
    return o;
  }
  explicit ApplyFsaBy(int alg, int poplimit=200);
  ApplyFsaBy(std::string const& name, int poplimit=200);
  ApplyFsaBy(const ApplyFsaBy &o) : algorithm(o.algorithm) {  }
  static std::string all_names(); // space separated
};

// in case you might want the CFG whether or not you apply FSA models:
struct HgCFG {
  HgCFG(Hypergraph const& ih) : ih(ih) { have_cfg=false; }
  Hypergraph const& ih;
  CFG cfg;
  bool have_cfg;
  void InitCFG(CFG &to) {
    to.Init(ih,true,false,true);
  }

  CFG &GetCFG()
  {
    if (!have_cfg) {
      have_cfg=true;
      InitCFG(cfg);
    }
    return cfg;
  }
  void GiveCFG(CFG &to) {
    if (!have_cfg)
      InitCFG(to);
    else {
      have_cfg=false;
      to.Clear();
      to.Swap(cfg);
    }
  }
  CFG const& GetCFG() const {
    assert(have_cfg);
    return cfg;
  }
};


void ApplyFsaModels(HgCFG &hg_or_cfg_in,
                    const SentenceMetadata& smeta,
                    const FsaFeatureFunction& fsa,
                    DenseWeightVector const& weights, // pre: in is weighted by these (except with fsa featval=0 before this)
                    ApplyFsaBy const& cfg,
                    Hypergraph* out);

inline void ApplyFsaModels(Hypergraph const& ih,
                    const SentenceMetadata& smeta,
                    const FsaFeatureFunction& fsa,
                    DenseWeightVector const& weights, // pre: in is weighted by these (except with fsa featval=0 before this)
                    ApplyFsaBy const& cfg,
                    Hypergraph* out) {
  HgCFG i(ih);
  ApplyFsaModels(i,smeta,fsa,weights,cfg,out);
}


#endif
