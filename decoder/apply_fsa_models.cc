#include "apply_fsa_models.h"
#include "hg.h"
#include "ff_fsa_dynamic.h"
#include "ff_from_fsa.h"
#include "feature_vector.h"
#include "stringlib.h"
#include "apply_models.h"
#include <stdexcept>
#include <cassert>
#include "cfg.h"

using namespace std;

struct ApplyFsa {
  ApplyFsa(const Hypergraph& ih,
           const SentenceMetadata& smeta,
           const FsaFeatureFunction& fsa,
           DenseWeightVector const& weights,
           ApplyFsaBy const& by,
           Hypergraph* oh)
    :ih(ih),smeta(smeta),fsa(fsa),weights(weights),by(by),oh(oh)
  {
//    sparse_to_dense(weight_vector,&weights);
    Init();
  }
  void Init() {
    if (by.IsBottomUp())
      ApplyBottomUp();
    else
      ApplyEarley();
  }
  void ApplyBottomUp() {
    assert(by.IsBottomUp());
    FeatureFunctionFromFsa<FsaFeatureFunctionFwd> buff(&fsa);
    buff.Init(); // mandatory to call this (normally factory would do it)
    vector<const FeatureFunction*> ffs(1,&buff);
    ModelSet models(weights, ffs);
    IntersectionConfiguration i(by.BottomUpAlgorithm(),by.pop_limit);
    ApplyModelSet(ih,smeta,models,i,oh);
  }
  void ApplyEarley() {
    CFG cfg(ih,true,false,true);
  }
private:
  const Hypergraph& ih;
  const SentenceMetadata& smeta;
  const FsaFeatureFunction& fsa;
//  WeightVector weight_vector;
  DenseWeightVector weights;
  ApplyFsaBy by;
  Hypergraph* oh;
};


void ApplyFsaModels(const Hypergraph& ih,
                    const SentenceMetadata& smeta,
                    const FsaFeatureFunction& fsa,
                    DenseWeightVector const& weight_vector,
                    ApplyFsaBy const& by,
                    Hypergraph* oh)
{
  ApplyFsa a(ih,smeta,fsa,weight_vector,by,oh);
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

