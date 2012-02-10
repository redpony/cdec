#ifndef CDEC_CFG_H
#define CDEC_CFG_H

#define DVISITRULEID(x)

// for now, debug means remembering and printing the TRule behind each CFG rule
#ifndef CFG_DEBUG
# define CFG_DEBUG 1
#endif
#ifndef CFG_KEEP_TRULE
# define CFG_KEEP_TRULE 0
#endif

#if CFG_DEBUG
# define IF_CFG_DEBUG(x) x;
#else
# define IF_CFG_DEBUG(x)
#endif

#if CFG_KEEP_TRULE
# define IF_CFG_TRULE(x) x;
#else
# define IF_CFG_TRULE(x)
#endif

/* for target FSA intersection, we want to produce a simple (feature weighted) CFG using the target projection of a hg.  this is essentially isomorphic to the hypergraph, and we're copying part of the rule info (we'll maintain a pointer to the original hg edge for posterity/debugging; and perhaps avoid making a copy of the feature vector).  but we may also want to support CFG read from text files (w/ features), without needing to have a backing hypergraph.  so hg pointer may be null?  multiple types of CFG?  always copy the feature vector?  especially if we choose to binarize, we won't want to rely on 1:1 alignment w/ hg

   question: how much does making a copy (essentially) of hg simplify things?  is the space used worth it?  is the node in/out edges index really that much of a waste?  is the use of indices that annoying?

   answer: access to the source side and target side rhs is less painful - less indirection; if not a word (w>0) then -w is the NT index.  also, non-synchronous ops like binarization make sense.  hg is a somewhat bulky encoding of non-synchronous forest

   using indices to refer to NTs saves space (32 bit index vs 64 bit pointer) and allows more efficient ancillary maps for e.g. chart info (if we used pointers to actual node structures, it would be tempting to add various void * or other slots for use by mapped-during-computation ephemera)
 */

#include <sstream>
#include <string>
#include <vector>
#include "feature_vector.h"
#include "small_vector.h"
#include "wordid.h"
#include "tdict.h"
#include "trule.h"
#include "prob.h"
//#include "int_or_pointer.h"
#include "small_vector.h"
#include "nt_span.h"
#include <algorithm>
#include "indices_after.h"
#include <boost/functional/hash.hpp>

class Hypergraph;
class CFGFormat; // #include "cfg_format.h"
class CFGBinarize; // #include "cfg_binarize.h"

#undef CFG_MUST_EQ
#define CFG_MUST_EQ(f) if (!(o.f==f)) return false;

struct CFG {
  typedef int RuleHandle;
  typedef int NTHandle;
  typedef SmallVector<WordID> RHS; // same as in trule rhs: >0 means token, <=0 means -node index (not variable index)
  typedef std::vector<RuleHandle> Ruleids;

  void print_nt_name(std::ostream &o,NTHandle n) const {
    o << nts[n].from << n;
  }
  std::string nt_name(NTHandle n) const {
    std::ostringstream o;
    print_nt_name(o,n);
    return o.str();
  }
  void print_rhs_name(std::ostream &o,WordID w) const {
    if (w<=0) print_nt_name(o,-w);
    else o<<TD::Convert(w);
  }
  std::string rhs_name(WordID w) const {
    if (w<=0) return nt_name(-w);
    else return TD::Convert(w);
  }
  static void static_print_nt_name(std::ostream &o,NTHandle n) {
    o<<'['<<n<<']';
  }
  static std::string static_nt_name(NTHandle w) {
    std::ostringstream o;
    static_print_nt_name(o,w);
    return o.str();
  }
  static void static_print_rhs_name(std::ostream &o,WordID w) {
    if (w<=0) static_print_nt_name(o,-w);
    else o<<TD::Convert(w);
  }
  static std::string static_rhs_name(WordID w) {
    std::ostringstream o;
    static_print_rhs_name(o,w);
    return o.str();
  }

  typedef std::pair<WordID,WordID> BinRhs;

  struct Rule {
    std::size_t hash_impl() const {
      using namespace boost;
      std::size_t h=lhs;
      hash_combine(h,rhs);
      hash_combine(h,p);
      hash_combine(h,f);
      return h;
    }
    bool operator ==(Rule const &o) const {
      CFG_MUST_EQ(lhs)
      CFG_MUST_EQ(rhs)
      CFG_MUST_EQ(p)
      CFG_MUST_EQ(f)
      return true;
    }
    inline bool operator!=(Rule const& o) const { return !(o==*this); }

    // for binarizing - no costs/probs
    Rule() : lhs(-1) {  }
    bool is_null() const { return lhs<0; }
    void set_null() { lhs=-1; rhs.clear();f.clear(); IF_CFG_TRULE(rule.reset();) }

    Rule(int lhs,BinRhs const& binrhs) : lhs(lhs),rhs(2),p(1) {
      rhs[0]=binrhs.first;
      rhs[1]=binrhs.second;
    }
    Rule(int lhs,RHS const& rhs) : lhs(lhs),rhs(rhs),p(1) {
    }

    int lhs; // index into nts
    RHS rhs;
    prob_t p; // h unused for now (there's nothing admissable, and p is already using 1st pass inside as pushed toward top)
    FeatureVector f; // may be empty, unless copy_features on Init
    IF_CFG_TRULE(TRulePtr rule;)
    int size() const { // for stats only
      return rhs.size();
    }
    void Swap(Rule &o) {
      using namespace std;
      swap(lhs,o.lhs);
      swap(rhs,o.rhs);
      swap(p,o.p);
      swap(f,o.f);
      IF_CFG_TRULE(swap(rule,o.rule);)
    }
    friend inline void swap(Rule &a,Rule &b) {
      a.Swap(b);
    }

    template<class V>
    void visit_rhs_nts(V &v) const {
      for (RHS::const_iterator i=rhs.begin(),e=rhs.end();i!=e;++i) {
        WordID w=*i;
        if (w<=0)
          v(-w);
      }
    }
    template<class V>
    void visit_rhs_nts(V const& v) const {
      for (RHS::const_iterator i=rhs.begin(),e=rhs.end();i!=e;++i) {
        WordID w=*i;
        if (w<=0)
          v(-w);
      }
    }

    template<class V>
    void visit_rhs(V &v) const {
      for (RHS::const_iterator i=rhs.begin(),e=rhs.end();i!=e;++i) {
        WordID w=*i;
        if (w<=0)
          v.visit_nt(-w);
        else
          v.visit_t(w);
      }
    }

    // returns 0 or 1 (# of non null rules in this rule).
    template <class O>
    bool reorder_from(O &order,NTHandle removed=-1) {
      for (RHS::iterator i=rhs.begin(),e=rhs.end();i!=e;++i) {
        WordID &w=*i;
        if (w<=0) {
          int oldnt=-w;
          NTHandle newnt=(NTHandle)order[oldnt]; // e.g. unsigned to int (-1) conversion should be ok
          if (newnt==removed) {
            set_null();
            return false;
          }
          w=-newnt;
        }
      }
      return true;
    }
  };

  struct NT {
    NT() {  }
    explicit NT(RuleHandle r) : ruleids(1,r) {  }
    std::size_t hash_impl() const { using namespace boost; return hash_value(ruleids); }
    bool operator ==(NT const &o) const {
      return ruleids==o.ruleids; // don't care about from
    }
    inline bool operator!=(NT const& o) const { return !(o==*this); }
    Ruleids ruleids; // index into CFG rules with lhs = this NT.  aka in_edges_
    NTSpan from; // optional name - still needs id to disambiguate
    void Swap(NT &o) {
      using namespace std;
      swap(ruleids,o.ruleids);
      swap(from,o.from);
    }
    friend inline void swap(NT &a,NT &b) {
      a.Swap(b);
    }
  };

  CFG() : hg_() { uninit=true; }

  // provided hg will have weights pushed up to root
  CFG(Hypergraph const& hg,bool target_side=true,bool copy_features=false,bool push_weights=true) {
    Init(hg,target_side,copy_features,push_weights);
  }
  bool Uninitialized() const { return uninit; }
  void Clear();
  bool Empty() const { return nts.empty(); }
  void UnindexRules(); // save some space?
  void ReindexRules(); // scan over rules and rebuild NT::ruleids (e.g. after using UniqRules)
  int UniqRules(NTHandle ni); // keep only the highest prob rule for each rhs and lhs=nt - doesn't remove from Rules; just removes from nts[ni].ruleids.  keeps the same order in this sense: for a given signature (rhs), that signature's first representative in the old ruleids will become the new position of the best.  as a consequence, if you SortLocalBestFirst() then UniqRules(), the result is still best first.  but you may also call this on unsorted ruleids.  returns number of rules kept
  inline int UniqRules() {
    int nkept=0;
    for (int i=0,e=nts.size();i!=e;++i) nkept+=UniqRules(i);
    return nkept;
  }
  int rules_size() const {
    const int sz=rules.size();
    int sum=sz;
    for (int i=0;i<sz;++i)
      sum+=rules[i].size();
    return sum;
  }

  void SortLocalBestFirst(NTHandle ni); // post: nts[ni].ruleids lists rules from highest p to lowest.  when doing best-first earley intersection/parsing, you don't want to use the global marginal viterbi; you want to ignore outside in ordering edges for a node, so call this.  stable in case of ties
  inline void SortLocalBestFirst() {
    for (int i=0,e=nts.size();i!=e;++i) SortLocalBestFirst(i);
  }
  void Init(Hypergraph const& hg,bool target_side=true,bool copy_features=false,bool push_weights=true);
  void Print(std::ostream &o,CFGFormat const& format) const; // see cfg_format.h
  void Print(std::ostream &o) const; // default format
  void PrintRule(std::ostream &o,RuleHandle rulei,CFGFormat const& format) const;
  void PrintRule(std::ostream &o,RuleHandle rulei) const;
  std::string ShowRule(RuleHandle rulei) const;
  void Swap(CFG &o) { // make sure this includes all fields (easier to see here than in .cc)
    using namespace std;
    swap(uninit,o.uninit);
    swap(hg_,o.hg_);
    swap(goal_inside,o.goal_inside);
    swap(pushed_inside,o.pushed_inside);
    swap(rules,o.rules);
    swap(nts,o.nts);
    swap(goal_nt,o.goal_nt);
  }

  //NOTE: this checks exact equality of data structures only.  it's well known that CFG equivalence (and intersection==empty) test is undecidable.
  bool operator ==(CFG const &o) const {
    // doesn't matter: hg, goal_inside
    CFG_MUST_EQ(uninit)
    CFG_MUST_EQ(pushed_inside)
    CFG_MUST_EQ(goal_nt)
    CFG_MUST_EQ(nts)
    CFG_MUST_EQ(rules)
    return true;
  }
  inline bool operator!=(CFG const& o) const { return !(o==*this); }

  typedef std::vector<NTHandle> NTOrder; // a list of nts, in definition-before-use order.

  //perhaps: give up on templated Order and move the below to .cc (NTOrder should be fine)

  // post: iterating nts 0,1... the same as order[0],order[1],... ; return number of non-null rules (these aren't actually deleted)
  // pre: order is (without duplicates) a range of NTHandle
  template <class Order>
  int ReorderNTs(Order const& order) {
    using namespace std;
    int nn=nts.size();
#if 0
    NTs newnts(order.size()); // because the (sub)permutation order may have e.g. 1<->4
    int ni=0;
    for (typename Order::const_iterator i=order.begin(),e=order.end();i!=e;++i) {
      assert(*i<nn);
      swap(newnts[ni++],nts[*i]);
    }
    swap(newnts,nts);
#endif
    indices_after remap_nti;
    remap_nti.init_inverse_order(nn,order);
    remap_nti.do_moves_swap(nts);// (equally efficient (or more?) than the disabled nt swapping above.
    goal_nt=remap_nti.map[goal_nt]; // remap goal, of course
    // fix rule ids
    return RemapRules(remap_nti.map,(NTHandle)indices_after::REMOVED);
  }

  // return # of kept rules (not null)
  template <class NTHandleRemap>
  int RemapRules(NTHandleRemap const& remap_nti,NTHandle removed=-1) {
    int n_non_null=0;
    for (int i=0,e=rules.size();i<e;++i)
      n_non_null+=rules[i].reorder_from(remap_nti,removed);
    return n_non_null;
  }

  // call after rules are indexed.
  template <class V>
  void VisitRuleIds(V &v) {
    for (int i=0,e=nts.size();i<e;++i) {
      SHOWM(DVISITRULEID,"VisitRuleIds nt",i);
      for (Ruleids::const_iterator j=nts[i].ruleids.begin(),jj=nts[i].ruleids.end();j!=jj;++j) {
        SHOWM2(DVISITRULEID,"VisitRuleIds",i,*j);
        v(*j);
      }
    }

  }
  template <class V>
  void VisitRuleIds(V const& v) {
    for (int i=0,e=nts.size();i<e;++i)
      for (Ruleids::const_iterator j=nts[i].ruleids.begin(),jj=nts[i].ruleids.end();j!=jj;++j)
        v(*j);
  }

  // no index needed
  template <class V>
  void VisitRulesUnindexed(V const &v) {
    for (int i=0,e=rules.size();i<e;++i)
      if (!rules[i].is_null())
        v(i,rules[i]);
  }



  void OrderNTsTopo(NTOrder *o,std::ostream *cycle_complain=0); // places NTs in defined (completely) bottom-up before use order.  this is actually reverse topo order considering edges from lhs->rhs.
  // you would need to do this only if you didn't build from hg, or you Binarize without bin_topo option.
  // note: this doesn't sort the list of rules; it's assumed that if you care about the topo order you'll iterate over nodes.
  // cycle_complain means to warn in case of back edges.  it's not necessary to prevent inf. loops.  you get some order that's not topo if there are loops.  starts from goal_nt, of course.

  void OrderNTsTopo(std::ostream *cycle_complain=0) {
    NTOrder o;
    OrderNTsTopo(&o,cycle_complain);
    ReorderNTs(o);
  }

  void BinarizeL2R(bool bin_unary=false,bool name_nts=false);
  void Binarize(CFGBinarize const& binarize_options); // see cfg_binarize.h for docs
  void BinarizeSplit(CFGBinarize const& binarize_options);
  void BinarizeThresh(CFGBinarize const& binarize_options); // maybe unbundle opts later

  typedef std::vector<NT> NTs;
  NTs nts;
  typedef std::vector<Rule> Rules;
  Rules rules;
  int goal_nt;
  prob_t goal_inside,pushed_inside; // when we push viterbi weights to goal, we store the removed probability in pushed_inside
protected:
  bool uninit;
  Hypergraph const* hg_; // shouldn't be used for anything, esp. after binarization
  // rules/nts will have same index as hg edges/nodes
};

inline std::size_t hash_value(CFG::Rule const& r) {
  return r.hash_impl();
}

inline std::size_t hash_value(CFG::NT const& r) {
  return r.hash_impl();
}

inline void swap(CFG &a,CFG &b) {
  a.Swap(b);
}

std::ostream &operator<<(std::ostream &o,CFG const &x);

#endif
