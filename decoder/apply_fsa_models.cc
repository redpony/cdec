//see apply_fsa_models.README for notes on the l2r earley fsa+cfg intersection
//implementation in this file (also some comments in this file)
#define SAFE_VALGRIND 1

#include "apply_fsa_models.h"
#include <stdexcept>
#include <cassert>
#include <queue>
#include <stdint.h>

#include "writer.h"
#include "hg.h"
#include "ff_fsa_dynamic.h"
#include "ff_from_fsa.h"
#include "feature_vector.h"
#include "stringlib.h"
#include "apply_models.h"
#include "cfg.h"
#include "hg_cfg.h"
#include "utoa.h"
#include "hash.h"
#include "value_array.h"
#include "d_ary_heap.h"
#include "agenda.h"
#include "show.h"
#include "string_to.h"


#define DFSA(x) x
//fsa earley chart

#define DPFSA(x) x
//prefix trie

#define DBUILDTRIE(x)

#define PRINT_PREFIX 1
#if PRINT_PREFIX
# define IF_PRINT_PREFIX(x) x
#else
# define IF_PRINT_PREFIX(x)
#endif
// keep backpointers in prefix trie so you can print a meaningful node id

static const unsigned FSA_AGENDA_RESERVE=10; // TODO: increase to 1<<24 (16M)

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

I'm using option 1.
*/

// if we don't greedy-binarize, we want to encode recognized prefixes p (X -> p . rest) efficiently.  if we're doing this, we may as well also push costs so we can best-first select rules in a lazy fashion.  this is effectively left-branching binarization, of course.

template <class K,class V,class Hash>
struct fsa_map_type {
  typedef std::map<K,V> type; // change to HASH_MAP ?
};
//template typedef - and macro to make it less painful
#define FSA_MAP(k,v) fsa_map_type<k,v,boost::hash<k> >::type

struct PrefixTrieNode;
typedef PrefixTrieNode *NodeP;
typedef PrefixTrieNode const *NodePc;

// for debugging prints only
struct TrieBackP {
  WordID w;
  NodePc from;
  TrieBackP(WordID w=0,NodePc from=0) : w(w),from(from) {  }
};

FsaFeatureFunction const* print_fsa=0;
CFG const* print_cfg=0;
inline ostream& print_cfg_rhs(std::ostream &o,WordID w,CFG const*pcfg=print_cfg) {
  if (pcfg)
    pcfg->print_rhs_name(o,w);
  else
    CFG::static_print_rhs_name(o,w);
  return o;
}

inline std::string nt_name(WordID n,CFG const*pcfg=print_cfg) {
  if (pcfg) return pcfg->nt_name(n);
  return CFG::static_nt_name(n);
}

template <class V>
ostream& print_by_nt(std::ostream &o,V const& v,CFG const*pcfg=print_cfg,char const* header="\nNT -> X\n") {
  o<<header;
  for (int i=0;i<v.size();++i)
    o << nt_name(i,pcfg) << " -> "<<v[i]<<"\n";
  return o;
}

template <class V>
ostream& print_map_by_nt(std::ostream &o,V const& v,CFG const*pcfg=print_cfg,char const* header="\nNT -> X\n") {
  o<<header;
  for (typename V::const_iterator i=v.begin(),e=v.end();i!=e;++i) {
    print_cfg_rhs(o,i->first,pcfg) << " -> "<<i->second<<"\n";
  }
  return o;
}

struct PrefixTrieEdge {
  PrefixTrieEdge()
  //    : dest(0),w(TD::max_wordid)
  {}
  PrefixTrieEdge(WordID w,NodeP dest)
    : dest(dest),w(w)
  {}
//  explicit PrefixTrieEdge(best_t p) : p(p),dest(0) {  }

  best_t p;// viterbi additional prob, i.e. product over path incl. p_final = total rule prob.  note: for final edge, set this.
  //DPFSA()
  // we can probably just store deltas, but for debugging remember the full p
  //    best_t delta; //
  NodeP dest;
  bool is_final() const { return dest==0; }
  best_t p_dest() const;
  WordID w; // for root and and is_final(), this will be (negated) NTHandle.

  // for sorting most probable first in adj; actually >(p)
  inline bool operator <(PrefixTrieEdge const& o) const {
    return o.p<p;
  }
  PRINT_SELF(PrefixTrieEdge)
  void print(std::ostream &o) const {
    print_cfg_rhs(o,w);
    o<<"{"<<p<<"}->"<<dest;
  }
};

//note: ending a rule is handled with a special final edge, so that possibility can be explored in best-first order along with the rest (alternative: always finish a final rule by putting it on queue).  this edge has no symbol on it.
struct PrefixTrieNode {
  best_t p; // viterbi (max prob) of rule this node leads to - when building.  telescope later onto edges for best-first.
//  bool final; // may also have successors, of course.  we don't really need to track this; a null dest edge in the adj list lets us encounter the fact in best first order.
  void p_delta(int next,best_t &p) const {
    p*=adj[next].p;
  }
  void inc_adj(int &next,best_t &p) const {
    p/=adj[next].p; //TODO: cache deltas
    ++next;
    p*=adj[next].p;
  }


  typedef TrieBackP BP;
  typedef std::vector<BP> BPs;
  void back_vec(BPs &ns) const {
    IF_PRINT_PREFIX(if(backp.from) { ns.push_back(backp); backp.from->back_vec(ns); })
  }

  BPs back_vec() const {
    BPs ret;
    back_vec(ret);
    return ret;
  }

  unsigned size() const {
    unsigned a=adj.size();
    unsigned e=edge_for.size();
    return a>e?a:e;
  }

  void print_back_str(std::ostream &o) const {
    BPs back=back_vec();
    unsigned i=back.size();
    if (!i) {
      o<<"PrefixTrieNode@"<<(uintptr_t)this;
      return;
    }
    bool first=true;
    while (i--<=0) {
      if (!first) o<<',';
      first=false;
      WordID w=back[i].w;
      print_cfg_rhs(o,w);
    }
  }
  std::string back_str() const {
    std::ostringstream o;
    print_back_str(o);
    return o.str();
  }

//  best_t p_final; // additional prob beyond what we already paid. while building, this is the total prob
// instead of storing final, we'll say that an edge with a NULL dest is a final edge.  this way it gets sorted into the list of adj.

  // instead of completed map, we have trie start w/ lhs.
  NTHandle lhs; // nonneg. - instead of storing this in Item.
  IF_PRINT_PREFIX(BP backp;)

  enum { ROOT=-1 };
  explicit PrefixTrieNode(NTHandle lhs=ROOT,best_t p=1) : p(p),lhs(lhs),IF_PRINT_PREFIX(backp()) {
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
      SHOWM2(DPFSA,"index_adj",i,e);
      m[e.w]=e;
    }
  }
  template <class PV>
  void index_lhs(PV &v) {
    for (int i=0,e=adj.size();i!=e;++i) {
      PrefixTrieEdge const& edge=adj[i];
      // assert(edge.p.is_1());  // actually, after done_building, e will have telescoped dest->p/p.
      NTHandle n=-edge.w;
      assert(n>=0);
//      SHOWM3(DPFSA,"index_lhs",i,edge,n);
      v[n]=edge.dest;
    }
  }

  template <class PV>
  void done_root(PV &v) {
    assert(is_root());
    SHOWM1(DBUILDTRIE,"done_root",OSTRF1(print_map_by_nt,edge_for));
    done_building_r(); //sets adj
    SHOWM1(DBUILDTRIE,"done_root",OSTRF1(print_by_nt,adj));
//    SHOWM1(DBUILDTRIE,done_root,adj);
//    index_adj(); // we want an index for the root node?.  don't think so - index_lhs handles it.  also we stopped clearing edge_for.
    index_lhs(v); // uses adj
  }

  // call only once.
  void done_building_r() {
    done_building();
    for (int i=0;i<adj.size();++i)
      if (adj[i].dest) // skip final edge
        adj[i].dest->done_building_r();
  }

  // for done_building; compute incremental (telescoped) edge p
  PrefixTrieEdge /*const&*/ operator()(PrefixTrieEdgeFor::value_type & pair) const {
    PrefixTrieEdge &e=pair.second;//const_cast<PrefixTrieEdge&>(pair.second);
    e.p=e.p_dest()/p;
    return e;
  }

  // call only once.
  void done_building() {
    SHOWM3(DBUILDTRIE,"done_building",edge_for.size(),adj.size(),1);
#if 1
    adj.reinit_map(edge_for,*this);
#else
    adj.reinit(edge_for.size());
    SHOWM3(DBUILDTRIE,"done_building_reinit",edge_for.size(),adj.size(),2);
    Adj::iterator o=adj.begin();
    for (PrefixTrieEdgeFor::iterator i=edge_for.begin(),e=edge_for.end();i!=e;++i) {
      SHOWM3(DBUILDTRIE,"edge_for",o-adj.begin(),i->first,i->second);
      PrefixTrieEdge &edge=i->second;
      edge.p=(edge.dest->p)/p;
      *o++=edge;
//      (*this)(*i);
    }
#endif
    SHOWM1(DBUILDTRIE,"done building adj",prange(adj.begin(),adj.end(),true));
    assert(adj.size()==edge_for.size());
//    if (final) p_final/=p;
    std::sort(adj.begin(),adj.end());
    //TODO: store adjacent differences on edges (compared to
  }

  typedef ValueArray<PrefixTrieEdge>  Adj;
//  typedef vector<PrefixTrieEdge> Adj;
  Adj adj;

  typedef WordID W;

  // let's compute p_min so that every rule reachable from the created node has p at least this low.
  NodeP improve_edge(PrefixTrieEdge const& e,best_t rulep) {
    NodeP d=e.dest;
    maybe_improve(d->p,rulep);
    return d;
  }

  inline NodeP build(W w,best_t rulep) {
    return build(lhs,w,rulep);
  }
  inline NodeP build_lhs(NTHandle n,best_t rulep) {
    return build(n,-n,rulep);
  }

  NodeP build(NTHandle lhs_,W w,best_t rulep) {
    PrefixTrieEdgeFor::iterator i=edge_for.find(w);
    if (i!=edge_for.end())
      return improve_edge(i->second,rulep);
    NodeP r=new PrefixTrieNode(lhs_,rulep);
    IF_PRINT_PREFIX(r->backp=BP(w,this));
//    edge_for.insert(i,PrefixTrieEdgeFor::value_type(w,PrefixTrieEdge(w,r)));
    add(edge_for,w,PrefixTrieEdge(w,r));
    SHOWM4(DBUILDTRIE,"built node",this,w,*r,r);
    return r;
  }

  void set_final(NTHandle lhs_,best_t pf) {
    assert(no_adj());
//    final=true;
    PrefixTrieEdge &e=edge_for[null_wordid];
    e.p=pf;
    e.dest=0;
    e.w=lhs_;
    maybe_improve(p,pf);
  }

private:
  void destroy_children() {
    assert(adj.size()>=edge_for.size());
    for (int i=0,e=adj.size();i<e;++i) {
      NodeP c=adj[i].dest;
      if (c) { // final state has no end
        delete c;
      }
    }
  }
public:
  ~PrefixTrieNode() {
    destroy_children();
  }
  void print(std::ostream &o) const {
    o << "Node"<<this<< ": "<<lhs << "->" << p;
    o << ',' << size() << ',';
    print_back_str(o);
  }
  PRINT_SELF(PrefixTrieNode)
};

inline best_t PrefixTrieEdge::p_dest() const {
  return dest ? dest->p : p; // for final edge, p was set (no sentinel node)
}


//Trie starts with lhs (nonneg index), then continues w/ rhs (mixed >0 word, else NT)
// trie ends with final edge, which points to a per-lhs prefix node
struct PrefixTrie {
  void print(std::ostream &o) const {
    o << cfgp << ' ' << root;
  }
  PRINT_SELF(PrefixTrie);
  CFG *cfgp;
  Rules const* rulesp;
  Rules const& rules() const { return *rulesp; }
  CFG const& cfg() const { return *cfgp; }
  PrefixTrieNode root;
  typedef std::vector<NodeP> LhsToTrie; // will have to check lhs2[lhs].p for best cost of some rule with that lhs, then use edge deltas after?  they're just caching a very cheap computation, really
  LhsToTrie lhs2; // no reason to use a map or hash table; every NT in the CFG will have some rule rhses.  lhs_to_trie[i]=root.edge_for[i], i.e. we still have a root trie node conceptually, we just access through this since it's faster.
  typedef LhsToTrie LhsToComplete;
  LhsToComplete lhs2complete; // the sentinel "we're completing" node (dot at end) for that lhs.  special case of suffix-set=same trie minimization (aka right branching binarization) // these will be used to track kbest completions, along with a l state (r state will be in the list)
  PrefixTrie(CFG &cfg) : cfgp(&cfg),rulesp(&cfg.rules),lhs2(cfg.nts.size(),0),lhs2complete(cfg.nts.size()) {
//    cfg.SortLocalBestFirst(); // instead we'll sort in done_building_r
    print_cfg=cfgp;
    SHOWM2(DBUILDTRIE,"PrefixTrie()",rulesp->size(),lhs2.size());
    cfg.VisitRuleIds(*this);
    root.done_root(lhs2);
    SHOWM3(DBUILDTRIE,"done w/ PrefixTrie: ",root,root.adj.size(),lhs2.size());
    DBUILDTRIE(print_by_nt(cerr,lhs2,cfgp));
    SHOWM1(DBUILDTRIE,"lhs2",OSTRF2(print_by_nt,lhs2,cfgp));
  }

  void operator()(int ri) {
    Rule const& r=rules()[ri];
    NTHandle lhs=r.lhs;
    best_t p=r.p;
//    NodeP n=const_cast<PrefixTrieNode&>(root).build_lhs(lhs,p);
    NodeP n=root.build_lhs(lhs,p);
    SHOWM4(DBUILDTRIE,"Prefixtrie rule id, root",ri,root,p,*n);
    for (RHS::const_iterator i=r.rhs.begin(),e=r.rhs.end();;++i) {
      SHOWM2(DBUILDTRIE,"PrefixTrie build or final",i-r.rhs.begin(),*n);
      if (i==e) {
        n->set_final(lhs,p);
        break;
      }
      n=n->build(*i,p);
      SHOWM2(DBUILDTRIE,"PrefixTrie built",*i,*n);
    }
//    root.build(lhs,r.p)->build(r.rhs,r.p);
  }
  inline NodeP lhs2_ex(NTHandle n) const {
    NodeP r=lhs2[n];
    if (!r) throw std::runtime_error("PrefixTrie: no CFG rule w/ lhs "+cfgp->nt_name(n));
    return r;
  }
private:
  PrefixTrie(PrefixTrie const& o);
};



typedef std::size_t ItemHash;


struct ItemKey {
  explicit ItemKey(NodeP start,Bytes const& start_state) : dot(start),q(start_state),r(start_state) {  }
  explicit ItemKey(NodeP dot) : dot(dot) {  }
  NodeP dot; // dot is a function of the stuff already recognized, and gives a set of suffixes y to complete to finish a rhs for lhs() -> dot y.  for a lhs A -> . *, this will point to lh2[A]
  Bytes q,r; // (q->r are the fsa states; if r is empty it means
  bool operator==(ItemKey const& o) const {
    return dot==o.dot && q==o.q && r==o.r;
  }
  inline ItemHash hash() const {
    ItemHash h=GOLDEN_MEAN_FRACTION*(ItemHash)(dot-NULL); // i.e. lower order bits of ptr are nonrandom
    using namespace boost;
    hash_combine(h,q);
    hash_combine(h,r);
    return h;
  }
  template<class O>
  void print(O &o) const {
    o<<"lhs="<<lhs();
    if (dot)
      dot->print_back_str(o);
    if (print_fsa) {
      o<<'/';
      print_fsa->print_state(o,&q[0]);
      o<<"->";
      print_fsa->print_state(o,&r[0]);
    }
  }
  NTHandle lhs() const { return dot->lhs; }
  PRINT_SELF(ItemKey)
};
inline ItemHash hash_value(ItemKey const& x) {
  return x.hash();
}
ItemKey null_item((PrefixTrieNode*)0);

struct Item;
typedef Item *ItemP;

/* we use a single type of item so it can live in a single best-first queue.  we hold them by pointer so they can have mutable state, e.g. priority/location, but also lists of predictions and kbest completions (i.e. completions[L,r] = L -> * (r,s), by 1best for each possible s.  we may discover more s later.  we could use different subtypes since we hold by pointer, but for now everything will be packed as variants of Item */
#undef INIT_LOCATION
#if D_ARY_TRACK_OUT_OF_HEAP
# define INIT_LOCATION                           , location(D_ARY_HEAP_NULL_INDEX)
#elif !defined(NDEBUG) || SAFE_VALGRIND
 // avoid spurious valgrind warning - FIXME: still complains???
# define INIT_LOCATION                           , location()
#else
# define INIT_LOCATION
#endif

// these should go in a global best-first queue
struct ItemPrio {
  // NOTE: sum = viterbi (max)
  ItemPrio() : priority(init_0()),inner(init_0()) {  }
  explicit ItemPrio(best_t priority) : priority(priority),inner(init_0()) {  }
  best_t priority; // includes inner prob. (forward)
  /* The forward probability alpha_i(X[k]->x.y) is the sum of the probabilities of all
     constrained paths of length i that end in state X[k]->x.y*/
  best_t inner;
  /* The inner probability beta_i(X[k]->x.y) is the sum of the probabilities of all
     paths of length i-k that start in state X[k,k]->.xy and end in X[k,i]->x.y, and generate the input symbols x[k,...,i-1] */
  template<class O>
  void print(O &o) const {
    o<<priority; // TODO: show/use inner?
  }
  typedef ItemPrio self_type;
  SELF_TYPE_PRINT
};

#define ITEM_TYPE(X,t)                              \
  X(t,ADJ,=-1)                                 \

#define ITEM_TYPE_TYPE ItemType

DECLARE_NAMED_ENUM(ITEM_TYPE)
DEFINE_NAMED_ENUM(ITEM_TYPE)

struct Item : ItemPrio,ItemKey {
/*  explicit Item(NodeP dot,best_t prio,int next) : ItemPrio(prio),ItemKey(dot),trienext(next),from(0)
                                        INIT_LOCATION
                                        {  }*/
//  ItemType t;
  // lazy queueing of succesors item:
  bool is_trie_adj() const {
    return trienext>=0;
  }
  explicit Item(FFState const& state,NodeP dot,best_t prio,int next=0) : ItemPrio(prio),ItemKey(dot,state),trienext(next),from(0)
                                                             INIT_LOCATION
  {
//    t=ADJ;
//    if (dot->adj.size())
    dot->p_delta(next,priority);
//    SHOWM1(DFSA,"Item(state,dot,prio)",prio);
  }
  typedef std::queue<ItemP> Predicted;
//  Predicted predicted; // this is empty, unless this is a predicted L -> .asdf item, or a to-complete L -> asdf .
  int trienext; // index of dot->adj to complete (if dest==0), or predict (if NT), or scan (if word).  note: we could store pointer inside adj since it and trie are @ fixed addrs.  less pointer arith, more space.
  ItemP from; //backpointer - 0 for L -> . asdf for the rest; L -> a .sdf, it's the L -> .asdf item.
  ItemP predicted_from() const {
    ItemP p=(ItemP)this;
    while(p->from) p=p->from;
    return p;
  }
  template<class O>
  void print(O &o) const {
    o<< '[';
    o<<this<<": ";
    ItemKey::print(o);
    o<<' ';
    ItemPrio::print(o);
    o<<" next="<<trienext;
    o<< ']';
  }
  PRINT_SELF(Item)
  unsigned location;
};

struct GetItemKey {
  typedef Item argument_type;
  typedef ItemKey result_type;
  result_type const& operator()(Item const& i) const { return i; }
  template <class T>
  T const& operator()(T const& t) const { return t; }
};

/* here's what i imagine (best first):
   all of these are looked up in a chart which includes the fsa states as part of the identity

   perhaps some items are ephemeral and never reused (e.g. edge items of a cube, where we delay traversing trie based on probabilities), but in all ohter cases we make entirely new objects derived from the original one (memoizing).  let's ignore lazier edge items for now and always push all successors onto heap.

   initial item (predicted): GOAL_NT -> . * (trie root for that lhs), start, start (fsa start states).  has a list of

   completing item ( L -> * . needs to queue all the completions immediately.  when predicting before a completion happens, add to prediction list.  after it happens, immediately use the completed bests.  this is confusing to me: the completions for an original NT w/ a given r state may end up in several different ones.  we don't only care about the 1 best cost r item but all the different r.

   the prediction's left/right uses the predictor's right

 */
template <class FsaFF=FsaFeatureFunction>
struct Chart {
  //typedef HASH_MAP<Item,ItemP,boost::hash<Item> > Items;
  //typedef Items::iterator FindItem;
  //typedef std::pair<FindItem,bool> InsertItem;
//  Items items;
  CFG &cfg; // TODO: remove this from Chart
  SentenceMetadata const& smeta;
  FsaFF const& fsa;
  NTHandle goal_nt;
  PrefixTrie trie;
  typedef Agenda<Item,BetterP,GetItemKey> A;
  A a;

  /* had to stop working on this for now - it's garbage/useless in this form - see NOTES.earley */

  // p_partial is priority*p(rule) - excluding the FSA model score, and predicted
  void succ(Item const& from,int adji,best_t p_partial) {
    PrefixTrieEdge const& te=from.dot->adj[adji];
    NodeP dest=te.dest;
    if (te.is_final()) {
      // complete
      return;
    }
    WordID w=te.w;
    if (w<0) {
      NTHandle lhs=-w;
    } else {

    }
  }

  void extend1() {
    BetterP better;
    Item &t=*a.top();
    best_t tp=t.priority;
    if (t.is_trie_adj()) {
      best_t pstop=a.second_best(); // remember; best_t a<b means a better than (higher prob) than b
//      NodeP d=t.dot;
      PrefixTrieNode::Adj const& adj=t.dot->adj;
      int n=t.trienext,m=adj.size();
      SHOWM3(DFSA,"popped",t,tp,pstop);
      for (;n<m;++n) { // cube corner
        PrefixTrieEdge const& te=adj[n];
        SHOWM3(DFSA,"maybe try trie next",n,te.p,pstop);
        if (better(te.p,pstop)) { // can get some improvement
          SHOWM2(DFSA,"trying adj ",m,te);
        } else {
          goto done;
        }
      }
      a.pop();
    done:;
    }
  }

  void best_first(unsigned kbest=1) {
    assert(kbest==1); //TODO: k-best via best-first requires revisiting best things again and adjusting desc.  tricky.
    while(!a.empty()) {
      extend1();
    }
  }

  Chart(CFG &cfg,SentenceMetadata const& smeta,FsaFF const& fsa,unsigned reserve=FSA_AGENDA_RESERVE)
    : cfg(cfg),smeta(smeta),fsa(fsa),trie(cfg),a(reserve) {
    assert(fsa.state_bytes());
    print_fsa=&fsa;
    goal_nt=cfg.goal_nt;
    best_t prio=init_1();
    SHOW1(DFSA,prio);
    a.add(a.construct(fsa.start,trie.lhs2_ex(goal_nt),prio));
  }
};


}//anon ns


DEFINE_NAMED_ENUM(FSA_BY)

template <class FsaFF=FsaFeatureFunction>
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
    stateless=!fsa.state_bytes();
  }
  void Compute() {
    if (by.IsBottomUp() || stateless)
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
  SentenceMetadata const& smeta;
  FsaFF const& fsa;
//  WeightVector weight_vector;
  DenseWeightVector weights;
  ApplyFsaBy by;
  Hypergraph* oh;
  std::string cfg_out;
  bool stateless;
};

template <class F>
void ApplyFsa<F>::ApplyBottomUp()
{
  assert(by.IsBottomUp());
  FeatureFunctionFromFsa<FsaFeatureFunctionFwd> buff(&fsa);
  buff.Init(); // mandatory to call this (normally factory would do it)
  vector<const FeatureFunction*> ffs(1,&buff);
  ModelSet models(weights, ffs);
  IntersectionConfiguration i(stateless ? BU_FULL : by.BottomUpAlgorithm(),by.pop_limit);
  ApplyModelSet(hgcfg.ih,smeta,models,i,oh);
}

template <class F>
void ApplyFsa<F>::ApplyEarley()
{
  hgcfg.GiveCFG(cfg);
  print_cfg=&cfg;
  print_fsa=&fsa;
  Chart<F> chart(cfg,smeta,fsa);
  // don't need to uniq - option to do that already exists in cfg_options
  //TODO:
  chart.best_first();
  *oh=hgcfg.ih;
}


void ApplyFsaModels(HgCFG &i,
                    const SentenceMetadata& smeta,
                    const FsaFeatureFunction& fsa,
                    DenseWeightVector const& weight_vector,
                    ApplyFsaBy const& by,
                    Hypergraph* oh)
{
  ApplyFsa<FsaFeatureFunction> a(i,smeta,fsa,weight_vector,by,oh);
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
