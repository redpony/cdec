#ifndef CFG_H
#define CFG_H

#ifndef CFG_DEBUG
# define CFG_DEBUG 1
#endif

/* for target FSA intersection, we want to produce a simple (feature weighted) CFG using the target projection of a hg.  this is essentially isomorphic to the hypergraph, and we're copying part of the rule info (we'll maintain a pointer to the original hg edge for posterity/debugging; and perhaps avoid making a copy of the feature vector).  but we may also want to support CFG read from text files (w/ features), without needing to have a backing hypergraph.  so hg pointer may be null?  multiple types of CFG?  always copy the feature vector?  especially if we choose to binarize, we won't want to rely on 1:1 alignment w/ hg

   question: how much does making a copy (essentially) of hg simplify things?  is the space used worth it?  is the node in/out edges index really that much of a waste?  is the use of indices that annoying?

   the only thing that excites me right now about an explicit cfg is that access to the target rhs can be less painful, and binarization *on the target side* is easier to define

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

class Hypergraph;
class CFG;



struct CFG {
  typedef int RuleHandle;
  typedef int NTHandle;
  typedef SmallVector<WordID> RHS; // same as in trule rhs: >0 means token, <=0 means -node index (not variable index)
  typedef std::vector<RuleHandle> Ruleids;

  struct Rule {
    int lhs; // index into nts
    RHS rhs;
    prob_t p; // h unused for now (there's nothing admissable, and p is already using 1st pass inside as pushed toward top)
    FeatureVector f; // may be empty, unless copy_features on Init
#if CFG_DEBUG
    TRulePtr rule; // normally no use for this (waste of space)
#endif
  };

  struct NT {
    Ruleids ruleids; // index into CFG rules with this lhs
  };

  CFG() : hg_() {  }

  // provided hg will have weights pushed up to root
  CFG(Hypergraph const& hg,bool target_side=true,bool copy_features=false,bool push_weights=true) {
    Init(hg,target_side,copy_features,push_weights);
  }
  void Init(Hypergraph const& hg,bool target_side=true,bool copy_features=false,bool push_weights=true);
protected:
  Hypergraph const* hg_; // shouldn't be used for anything, esp. after binarization
  prob_t goal_inside,pushed_inside; // when we push viterbi weights to goal, we store the removed probability in pushed_inside
  // rules/nts will have same index as hg edges/nodes
  typedef std::vector<Rule> Rules;
  Rules rules;
  typedef std::vector<NT> NTs;
  NTs nts;
};

#endif
