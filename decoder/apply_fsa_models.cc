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
#include "hash.h"
#include "value_array.h"

#define DFSA(x) x
#define DPFSA(x) x

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

/*

1) A -> x . * (trie)

this is somewhat nice.  cost pushed for best first, of course.  similar benefit as left-branching binarization without the explicit predict/complete steps?

vs. just

2) * -> x . y

here you have to potentially list out all A -> . x y as items * -> . x y immediately, and shared rhs seqs won't be shared except at the usual single-NT predict/complete.  of course, the prediction of items -> . x y can occur lazy best-first.

vs.

3) * -> x . *

with 3, we predict all sorts of useless items - that won't give us our goal A and may not partcipate in any parse.  this is not a good option at all.
*/

#define TRIE_START_LHS 1
// option 1) above.  0 would be option 3), which is dumb

// if we don't greedy-binarize, we want to encode recognized prefixes p (X -> p . rest) efficiently.  if we're doing this, we may as well also push costs so we can best-first select rules in a lazy fashion.  this is effectively left-branching binarization, of course.
template <class K,class V,class Hash>
struct fsa_map_type {
  typedef std::map<K,V> type;
};
//template typedef
#define FSA_MAP(k,v) fsa_map_type<k,v,boost::hash<k> >::type
typedef WordID LHS; // so this will be -NTHandle.


struct PrefixTrieNode {
  prob_t backward; // (viterbi) backward prob (for cost pushing)
#
#if TRIE_START_LHS
  typedef FSA_MAP(LHS,RuleHandle) Completed; // can only have one rule w/ a given signature (duplicates should be collapsed when making CFG).  but there may be multiple rules, with different LHS
  Completed completed;
#else
  bool complete; // may also have successors, of course
#endif
  // instead of completed map, we have trie start w/ lhs.?

  // outgoing edges will be ordered highest p to worst p
  struct Edge {
    DPFSA(prob_t p;) // we can probably just store deltas, but for debugging remember the full p
    prob_t delta; // p[i]=delta*p[i-1], with p(-1)=1
    PrefixTrieNode *dest;
    WordID w;
  };
  typedef FSA_MAP(WordID,Edge) BuildAdj;
  BuildAdj build_adj; //TODO: move builder elsewhere?
  typedef ValueArray<Edge>  Adj;
//  typedef vector<Edge> Adj;
  Adj adj;
  //TODO:
};

#if TRIE_START_LHS
//Trie starts with lhs, then continues w/ rhs
#else
// just rhs.  i think item names should exclude lhs if possible (most sharing).  get prefix cost w/ forward = viterbi (global best-first admissable h only) and it should be ok?
#endif

// costs are pushed.
struct PrefixTrie {
  CFG const* cfgp;
  CFG const& cfg() const { return *cfgp; }
  PrefixTrie(CFG const& cfg) : cfgp(&cfg) {

//TODO:
  }
};

// these should go in a global best-first queue
struct Item {
  prob_t forward;
  /* The forward probability alpha_i(X[k]->x.y) is the sum of the probabilities of all
     constrained paths of length that end in state X[k]->x.y*/
  prob_t inner;
  /* The inner probability beta_i(X[k]->x.y) is the sum of the probabilities of all
     paths of length i-k that start in state X[k,k]->.xy and end in X[k,i]->x.y, and generate the input symbols x[k,...,i-1] */

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
  cfg.SortLocalBestFirst();
  // don't need to uniq - option to do that already exists in cfg_options
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
