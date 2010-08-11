#ifndef CDEC_CFG_H
#define CDEC_CFG_H

// for now, debug means remembering and printing the TRule behind each CFG rule
#ifndef CFG_DEBUG
# define CFG_DEBUG 0
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

class Hypergraph;
class CFGFormat; // #include "cfg_format.h"
class CFGBinarize; // #include "cfg_binarize.h"

struct CFG {
  typedef int RuleHandle;
  typedef int NTHandle;
  typedef SmallVector<WordID> RHS; // same as in trule rhs: >0 means token, <=0 means -node index (not variable index)
  typedef std::vector<RuleHandle> Ruleids;

  void print_nt_name(std::ostream &o,NTHandle n) const {
    o << nts[n].from;
  }

  typedef std::pair<int,int> BinRhs;
  WordID BinName(BinRhs const& b);

  struct Rule {
    // for binarizing - no costs/probs
    Rule() {  }
    Rule(int lhs,BinRhs const& binrhs) : lhs(lhs),rhs(2),p(1) {
      rhs[0]=binrhs.first;
      rhs[1]=binrhs.second;
    }

    int lhs; // index into nts
    RHS rhs;
    prob_t p; // h unused for now (there's nothing admissable, and p is already using 1st pass inside as pushed toward top)
    FeatureVector f; // may be empty, unless copy_features on Init
#if CFG_DEBUG
    TRulePtr rule; // normally no use for this (waste of space)
#endif
    void Swap(Rule &o) {
      using namespace std;
      swap(lhs,o.lhs);
      swap(rhs,o.rhs);
      swap(p,o.p);
      swap(f,o.f);
#if CFG_DEBUG
      swap(rule,o.rule);
#endif
    }
  };

  struct NT {
    Ruleids ruleids; // index into CFG rules with lhs = this NT.  aka in_edges_
    NTSpan from; // optional name - still needs id to disambiguate
    void Swap(NT &o) {
      using namespace std;
      swap(ruleids,o.ruleids);
      swap(from,o.from);
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
  void Init(Hypergraph const& hg,bool target_side=true,bool copy_features=false,bool push_weights=true);
  void Print(std::ostream &o,CFGFormat const& format) const; // see cfg_format.h
  void PrintRule(std::ostream &o,RuleHandle rulei,CFGFormat const& format) const;
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
  void Binarize(CFGBinarize const& binarize_options);
protected:
  bool uninit;
  Hypergraph const* hg_; // shouldn't be used for anything, esp. after binarization
  prob_t goal_inside,pushed_inside; // when we push viterbi weights to goal, we store the removed probability in pushed_inside
  // rules/nts will have same index as hg edges/nodes
  typedef std::vector<Rule> Rules;
  Rules rules;
  typedef std::vector<NT> NTs;
  NTs nts;
  int goal_nt;
};

inline void swap(CFG &a,CFG &b) {
  a.Swap(b);
}


#endif
