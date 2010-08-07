#include "apply_fsa_models.h"
#include "hg.h"
#include "ff_fsa_dynamic.h"
#include "feature_vector.h"
#include "stringlib.h"
#include "apply_models.h"
#include <stdexcept>
#include <cassert>

using namespace std;

struct ApplyFsa {
  ApplyFsa(const Hypergraph& ih,
           const SentenceMetadata& smeta,
           const FsaFeatureFunction& fsa,
           DenseWeightVector const& weights,
           ApplyFsaBy const& cfg,
           Hypergraph* oh)
    :ih(ih),smeta(smeta),fsa(fsa),weights(weights),cfg(cfg),oh(oh)
  {
//    sparse_to_dense(weight_vector,&weights);
    Init();
  }
  void Init() {
    ApplyBottomUp();
    //TODO: implement l->r
  }
  void ApplyBottomUp() {
    assert(cfg.IsBottomUp());
    vector<const FeatureFunction*> ffs;
    ModelSet models(weights, ffs);
    IntersectionConfiguration i(cfg.BottomUpAlgorithm(),cfg.pop_limit);
    ApplyModelSet(ih,smeta,models,i,oh);
  }
private:
  const Hypergraph& ih;
  const SentenceMetadata& smeta;
  const FsaFeatureFunction& fsa;
//  WeightVector weight_vector;
  DenseWeightVector weights;
  ApplyFsaBy cfg;
  Hypergraph* oh;
};


void ApplyFsaModels(const Hypergraph& ih,
                    const SentenceMetadata& smeta,
                    const FsaFeatureFunction& fsa,
                    DenseWeightVector const& weight_vector,
                    ApplyFsaBy const& cfg,
                    Hypergraph* oh)
{
  ApplyFsa a(ih,smeta,fsa,weight_vector,cfg,oh);
}


namespace {
char const* anames[]={
  "BU_CUBE",
  "BU_FULL",
  "EARLEY",
  0
};
}

//TODO: named enum type in boost?

std::string ApplyFsaBy::name() const {
  return anames[algorithm];
}

std::string ApplyFsaBy::all_names() {
  std::ostringstream o;
  for (int i=0;i<N_ALGORITHMS;++i) {
    assert(anames[i]);
    if (i) o<<' ';
    o<<anames[i];
  }
  return o.str();
}

ApplyFsaBy::ApplyFsaBy(std::string const& n, int pop_limit) : pop_limit(pop_limit){
  algorithm=0;
  std::string uname=toupper(n);
  while(anames[algorithm] && anames[algorithm] != uname) ++algorithm;
  if (!anames[algorithm])
    throw std::runtime_error("Unknown ApplyFsaBy type: "+n+" - legal types: "+all_names());
}

ApplyFsaBy::ApplyFsaBy(int i, int pop_limit) : pop_limit(pop_limit) {
  assert (i>=0);
  assert (i<N_ALGORITHMS);
  algorithm=i;
}

int ApplyFsaBy::BottomUpAlgorithm() const {
  assert(IsBottomUp());
  return algorithm==BU_CUBE ?
    IntersectionConfiguration::CUBE
    :IntersectionConfiguration::FULL;
}

