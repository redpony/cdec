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
#include "hg_cfg.h"
#include "utoa.h"

using namespace std;

//impl details (not exported).  flat namespace for my ease.

typedef CFG::BinRhs BinRhs;
typedef CFG::NTs NTs;
typedef CFG::NT NT;
typedef CFG::NTHandle NTHandle;
typedef CFG::Rules Rules;
typedef CFG::Rule Rule;
typedef CFG::RuleHandle RuleHandle;

namespace {

// if we don't greedy-binarize, we want to encode recognized prefixes p (X -> p . rest) efficiently.  if we're doing this, we may as well also push costs so we can best-first select rules in a lazy fashion.  this is effectively left-branching binarization, of course.
template <class K,class V>
struct prefix_map_type {
  typedef std::map<K,V> type;
};
//template typedef
#define PREFIX_MAP(k,v) prefix_map_type<k,v>::type
typedef NTHandle LHS;
struct PrefixTrieNode {
  prob_t backward; // (viterbi) backward prob (for cost pushing)
  typedef PREFIX_MAP(LHS,RuleHandle) Completed; // can only have one rule w/ a given signature (duplicates should be collapsed when making CFG).  but there may be multiple rules, with different LHS
  Completed completed;
  typedef PREFIX_MAP(WordID,PrefixTrieNode *) Adj;
  Adj adj;
  //TODO:
};

struct PrefixTrie {
  CFG const* cfgp;
  CFG const& cfg() const { return *cfgp; }
  PrefixTrie(CFG const& cfg) : cfgp(&cfg) {
//TODO:
  }
};

}//anon ns


DEFINE_NAMED_ENUM(FSA_BY)

struct ApplyFsa {
  ApplyFsa(HgCFG &i,
           const SentenceMetadata& smeta,
           const FsaFeatureFunction& fsa,
           DenseWeightVector const& weights,
           ApplyFsaBy const& by,
           Hypergraph* oh
    )
    :hgcfg(i),smeta(smeta),fsa(fsa),weights(weights),by(by),oh(oh)
  {
  }
  void Compute() {
    if (by.IsBottomUp())
      ApplyBottomUp();
    else
      ApplyEarley();
  }
  void ApplyBottomUp();
  void ApplyEarley();
  CFG const& GetCFG();
private:
  CFG cfg;
  HgCFG &hgcfg;
  const SentenceMetadata& smeta;
  const FsaFeatureFunction& fsa;
//  WeightVector weight_vector;
  DenseWeightVector weights;
  ApplyFsaBy by;
  Hypergraph* oh;
  std::string cfg_out;
};

void ApplyFsa::ApplyBottomUp()
{
  assert(by.IsBottomUp());
  FeatureFunctionFromFsa<FsaFeatureFunctionFwd> buff(&fsa);
  buff.Init(); // mandatory to call this (normally factory would do it)
  vector<const FeatureFunction*> ffs(1,&buff);
  ModelSet models(weights, ffs);
  IntersectionConfiguration i(by.BottomUpAlgorithm(),by.pop_limit);
  ApplyModelSet(hgcfg.ih,smeta,models,i,oh);
}

void ApplyFsa::ApplyEarley()
{
  hgcfg.GiveCFG(cfg);
  //TODO:
}


void ApplyFsaModels(HgCFG &i,
                    const SentenceMetadata& smeta,
                    const FsaFeatureFunction& fsa,
                    DenseWeightVector const& weight_vector,
                    ApplyFsaBy const& by,
                    Hypergraph* oh)
{
  ApplyFsa a(i,smeta,fsa,weight_vector,by,oh);
  a.Compute();
}

/*
namespace {
char const* anames[]={
  "BU_CUBE",
  "BU_FULL",
  "EARLEY",
  0
};
}
*/

//TODO: named enum type in boost?

std::string ApplyFsaBy::name() const {
//  return anames[algorithm];
  return GetName(algorithm);
}

std::string ApplyFsaBy::all_names() {
  return FsaByNames(" ");
  /*
  std::ostringstream o;
  for (int i=0;i<N_ALGORITHMS;++i) {
    assert(anames[i]);
    if (i) o<<' ';
    o<<anames[i];
  }
  return o.str();
  */
}

ApplyFsaBy::ApplyFsaBy(std::string const& n, int pop_limit) : pop_limit(pop_limit) {
  std::string uname=toupper(n);
  algorithm=GetFsaBy(uname);
/*anames=0;
  while(anames[algorithm] && anames[algorithm] != uname) ++algorithm;
  if (!anames[algorithm])
    throw std::runtime_error("Unknown ApplyFsaBy type: "+n+" - legal types: "+all_names());
*/
}

ApplyFsaBy::ApplyFsaBy(FsaBy i, int pop_limit) : pop_limit(pop_limit) {
/*  if (i<0 || i>=N_ALGORITHMS)
    throw std::runtime_error("Unknown ApplyFsaBy type id: "+itos(i)+" - legal types: "+all_names());
*/
  GetName(i); // checks validity
  algorithm=i;
}

int ApplyFsaBy::BottomUpAlgorithm() const {
  assert(IsBottomUp());
  return algorithm==BU_CUBE ?
    IntersectionConfiguration::CUBE
    :IntersectionConfiguration::FULL;
}

void ApplyFsaModels(Hypergraph const& ih,
                    const SentenceMetadata& smeta,
                    const FsaFeatureFunction& fsa,
                    DenseWeightVector const& weights, // pre: in is weighted by these (except with fsa featval=0 before this)
                    ApplyFsaBy const& cfg,
                    Hypergraph* out)
{
  HgCFG i(ih);
  ApplyFsaModels(i,smeta,fsa,weights,cfg,out);
}
