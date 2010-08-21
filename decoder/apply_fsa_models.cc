#include "maybe_update_bound.h"
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
#include "d_ary_heap.h"
#include "agenda.h"

#define DFSA(x) x
#define DPFSA(x) x

using namespace std;

//impl details (not exported).  flat namespace for my ease.

typedef CFG::RHS RHS;
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
// 1 is option 1) above.  0 would be option 3), which is dumb

// if we don't greedy-binarize, we want to encode recognized prefixes p (X -> p . rest) efficiently.  if we're doing this, we may as well also push costs so we can best-first select rules in a lazy fashion.  this is effectively left-branching binarization, of course.
template <class K,class V,class Hash>
struct fsa_map_type {
  typedef std::map<K,V> type;
};
//template typedef
#define FSA_MAP(k,v) fsa_map_type<k,v,boost::hash<k> >::type
typedef WordID LHS; // so this will be -NTHandle.

struct get_second {
  template <class P>
  typename P::second_type const& operator()(P const& p) const {
    return p.second;
  }
};

struct PrefixTrieNode;
struct PrefixTrieEdge {
//  PrefixTrieEdge() {  }
//  explicit PrefixTrieEdge(prob_t p) : p(p),dest(0) {  }
  prob_t p;// viterbi additional prob, i.e. product over path incl. p_final = total rule prob
  //DPFSA()
  // we can probably just store deltas, but for debugging remember the full p
  //    prob_t delta; //
  PrefixTrieNode *dest;
  bool is_final() const { return dest==0; }
  WordID w; // for lhs, this will be nonneg NTHandle instead.  //  not set if is_final() // actually, set to lhs nt index

  // for sorting most probable first in adj; actually >(p)
  inline bool operator <(PrefixTrieEdge const& o) const {
    return o.p<p;
  }
};

struct PrefixTrieNode {
  prob_t p; // viterbi (max prob) of rule this node leads to - when building.  telescope later onto edges for best-first.
#if TRIE_START_LHS
//  bool final; // may also have successors, of course.  we don't really need to track this; a null dest edge in the adj list lets us encounter the fact in best first order.
//  prob_t p_final; // additional prob beyond what we already paid. while building, this is the total prob
// instead of storing final, we'll say that an edge with a NULL dest is a final edge.  this way it gets sorted into the list of adj.

  // instead of completed map, we have trie start w/ lhs.
  NTHandle lhs; // nonneg. - instead of storing this in Item.
#else
  typedef FSA_MAP(LHS,RuleHandle) Completed; // can only have one rule w/ a given signature (duplicates should be collapsed when making CFG).  but there may be multiple rules, with different LHS
  Completed completed;
#endif
  enum { ROOT=-1 };
  explicit PrefixTrieNode(NTHandle lhs=ROOT,prob_t p=1) : p(p),lhs(lhs) {
    //final=false;
  }
  bool is_root() const { return lhs==ROOT; } // means adj are the nonneg lhs indices, and we have the index edge_for still available

  // outgoing edges will be ordered highest p to worst p

  typedef FSA_MAP(WordID,PrefixTrieEdge) PrefixTrieEdgeFor;
public:
  PrefixTrieEdgeFor edge_for; //TODO: move builder elsewhere?  then need 2nd hash or edge include pointer to builder.  just clear this later
  bool have_adj() const {
    return adj.size()>=edge_for.size();
  }
  bool no_adj() const {
    return adj.empty();
  }

  void index_adj() {
    index_adj(edge_for);
  }
  template <class M>
  void index_adj(M &m) {
    assert(have_adj());
    m.clear();
    for (int i=0;i<adj.size();++i) {
      PrefixTrieEdge const& e=adj[i];
      m[e.w]=e;
    }
  }
  template <class PV>
  void index_root(PV &v) {
    v.resize(adj.size());
    for (int i=0,e=adj.size();i!=e;++i) {
      PrefixTrieEdge const& e=adj[i];
      // assert(e.p.is_1());  // actually, after done_building, e will have telescoped dest->p/p.
      v[e.w]=e.dest;
    }
  }

  // call only once.
  void done_building_r() {
    done_building();
    for (int i=0;i<adj.size();++i)
      adj[i].dest->done_building_r();
  }

  // for done_building; compute incremental (telescoped) edge p
  PrefixTrieEdge const& operator()(PrefixTrieEdgeFor::value_type const& pair) const {
    PrefixTrieEdge &e=const_cast<PrefixTrieEdge&>(pair.second);
    e.p=(e.dest->p)/p;
    return e;
  }

  // call only once.
  void done_building() {
    adj.reinit_map(edge_for,*this);
//    if (final) p_final/=p;
    std::sort(adj.begin(),adj.end());
    //TODO: store adjacent differences on edges (compared to
  }

  typedef ValueArray<PrefixTrieEdge>  Adj;
//  typedef vector<PrefixTrieEdge> Adj;
  Adj adj;

  typedef WordID W;

  // let's compute p_min so that every rule reachable from the created node has p at least this low.
  PrefixTrieNode *improve_edge(PrefixTrieEdge const& e,prob_t rulep) {
    PrefixTrieNode *d=e.dest;
    maybe_increase_max(d->p,rulep);
    return d;
  }

  inline PrefixTrieNode *build(W w,prob_t rulep) {
    return build(lhs,w,rulep);
  }
  inline PrefixTrieNode *build_lhs(NTHandle w,prob_t rulep) {
    return build(w,w,rulep);
  }

  PrefixTrieNode *build(NTHandle lhs_,W w,prob_t rulep) {
    PrefixTrieEdgeFor::iterator i=edge_for.find(w);
    if (i!=edge_for.end())
      return improve_edge(i->second,rulep);
    PrefixTrieEdge &e=edge_for[w];
    return e.dest=new PrefixTrieNode(lhs_,rulep);
  }

  void set_final(NTHandle lhs_,prob_t pf) {
    assert(no_adj());
//    final=true; // don't really need to track this.
    PrefixTrieEdge &e=edge_for[-1];
    e.p=pf;
    e.dest=0;
    e.w=lhs_;
    if (pf>p)
      p=pf;
  }

private:
  void destroy_children() {
    assert(adj.size()>=edge_for.size());
    for (int i=0,e=adj.size();i<e;++i) {
      PrefixTrieNode *c=adj[i].dest;
      if (c) { // final state has no end
        delete c;
      }
    }
  }
public:
  ~PrefixTrieNode() {
    destroy_children();
  }
};

#if TRIE_START_LHS
//Trie starts with lhs (nonneg index), then continues w/ rhs (mixed >0 word, else NT)
#else
// just rhs.  i think item names should exclude lhs if possible (most sharing).  get prefix cost w/ forward = viterbi (global best-first admissable h only) and it should be ok?
#endif

// costs are pushed.
struct PrefixTrie {
  CFG *cfgp;
  Rules const* rulesp;
  Rules const& rules() const { return *rulesp; }
  CFG const& cfg() const { return *cfgp; }
  PrefixTrieNode root;

  PrefixTrie(CFG &cfg) : cfgp(&cfg),rulesp(&cfg.rules) {
//    cfg.SortLocalBestFirst(); // instead we'll sort in done_building_r
    cfg.VisitRuleIds(*this);
    root.done_building_r();
    root.index_adj(); // maybe the index we use for building trie should be replaced by a much larger/faster table since we look up by lhs many times in parsing?
//TODO:
  }
  void operator()(int ri) const {
    Rule const& r=rules()[ri];
    NTHandle lhs=r.lhs;
    prob_t p=r.p;
    PrefixTrieNode *n=const_cast<PrefixTrieNode&>(root).build_lhs(lhs,p);
    for (RHS::const_iterator i=r.rhs.begin(),e=r.rhs.end();;++i) {
      if (i==e) {
        n->set_final(lhs,p);
        break;
      }
      n=n->build(*i,p);
    }
//    root.build(lhs,r.p)->build(r.rhs,r.p);
  }


};

typedef std::size_t ItemHash;

struct Item {
  explicit Item(PrefixTrieNode *dot,int next=0) : dot(dot),next(next) {  }
  PrefixTrieNode *dot; // dot is a function of the stuff already recognized, and gives a set of suffixes y to complete to finish a rhs for lhs() -> dot y.  for a lhs A -> . *, this will point to lh2[A]
  int next; // index of dot->adj to complete (if dest==0), or predict (if NT), or scan (if word)
  NTHandle lhs() const { return dot->lhs; }
  inline ItemHash hash() const {
    return GOLDEN_MEAN_FRACTION*next^((ItemHash)dot>>4); // / sizeof(PrefixTrieNode), approx., i.e. lower order bits of ptr are nonrandom
  }
};

inline ItemHash hash_value(Item const& x) {
  return x.hash();
}

Item null_item((PrefixTrieNode*)0);

// these should go in a global best-first queue
struct ItemP {
  ItemP() : forward(init_0()),inner(init_0()) {  }
  prob_t forward; // includes inner prob.
  // NOTE: sum = viterbi (max)
  /* The forward probability alpha_i(X[k]->x.y) is the sum of the probabilities of all
     constrained paths of length that end in state X[k]->x.y*/
  prob_t inner;
  /* The inner probability beta_i(X[k]->x.y) is the sum of the probabilities of all
     paths of length i-k that start in state X[k,k]->.xy and end in X[k,i]->x.y, and generate the input symbols x[k,...,i-1] */
};

struct Chart {
  //Agenda<Item> a;
  //typedef HASH_MAP<Item,ItemP,boost::hash<Item> > Items;
  //typedef Items::iterator FindItem;
  //typedef std::pair<FindItem,bool> InsertItem;
//  Items items;
  CFG &cfg; // TODO: remove this from Chart
  NTHandle goal_nt;
  PrefixTrie trie;
  typedef std::vector<PrefixTrieNode *> LhsToTrie; // will have to check lhs2[lhs].p for best cost of some rule with that lhs, then use edge deltas after?  they're just caching a very cheap computation, really
  LhsToTrie lhs2; // no reason to use a map or hash table; every NT in the CFG will have some rule rhses.  lhs_to_trie[i]=root.edge_for[i], i.e. we still have a root trie node conceptually, we just access through this since it's faster.

  void enqueue(Item const& item,ItemP const& p) {
//    FindItem f=items.find(item);
//    if (f==items.end()) ;

  }

  Chart(CFG &cfg) :cfg(cfg),trie(cfg) {
    goal_nt=cfg.goal_nt;
    trie.root.index_root(lhs2);
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
  Chart chart(cfg);
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
