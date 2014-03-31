#ifndef TREE_FRAGMENT
#define TREE_FRAGMENT

#include <deque>
#include <iostream>
#include <vector>
#include <string>

#include "tdict.h"

namespace cdec {

class BreadthFirstIterator;

static const unsigned LHS_BIT         = 0x10000000u;
static const unsigned RHS_BIT         = 0x20000000u;
static const unsigned FRONTIER_BIT    = 0x40000000u;
static const unsigned RESERVED_BIT    = 0x80000000u;
static const unsigned ALL_MASK        = 0x0FFFFFFFu;

inline bool IsNT(unsigned x) {
  return (x & (LHS_BIT | RHS_BIT | FRONTIER_BIT));
}

inline bool IsLHS(unsigned x) {
  return (x & LHS_BIT);
}

inline bool IsRHS(unsigned x) {
  return (x & RHS_BIT);
}

inline bool IsFrontier(unsigned x) {
  return (x & FRONTIER_BIT);
}

inline bool IsTerminal(unsigned x) {
  return (x & ALL_MASK) == x;
}

struct TreeFragmentProduction {
  TreeFragmentProduction() {}
  TreeFragmentProduction(int nttype, const std::vector<unsigned>& r) : lhs(nttype), rhs(r) {}
  unsigned lhs;
  std::vector<unsigned> rhs;
};

// this data structure represents a tree or forest
// productions can have mixtures of terminals and nonterminal symbols and non-terminal frontier sites
class TreeFragment {
 public:
  TreeFragment() : frontier_sites(), terminals() {}
  // (S (NP a (X b) c d) (VP (V foo) (NP (NN bar))))
  explicit TreeFragment(const std::string& tree, bool allow_frontier_sites = false);
  void DebugRec(unsigned cur, std::ostream* out) const;
  typedef BreadthFirstIterator iterator;
  typedef ptrdiff_t difference_type;
  typedef unsigned value_type;
  typedef const unsigned * pointer;
  typedef const unsigned & reference;

  iterator begin() const;
  iterator begin(unsigned node_idx) const;
  iterator end() const;

 private:
  // cp is the character index in the tree
  // np keeps track of the nodes (nonterminals) that have been built
  // symp keeps track of the terminal symbols that have been built
  void ParseRec(const std::string& tree, bool afs, unsigned cp, unsigned symp, unsigned np, unsigned* pcp, unsigned* psymp, unsigned* pnp);
 public:
  unsigned root;
  unsigned char frontier_sites;
  unsigned short terminals;

  std::vector<TreeFragmentProduction> nodes;
};

struct TFIState {
  TFIState() : node(), rhspos(), state() {}
  TFIState(unsigned n, unsigned p, unsigned s) : node(n), rhspos(p), state(s) {}
  bool operator==(const TFIState& o) const { return node == o.node && rhspos == o.rhspos && state == o.state; }
  bool operator!=(const TFIState& o) const { return node != o.node || rhspos != o.rhspos || state != o.state; }
  unsigned short node;
  unsigned short rhspos;
  unsigned char state;
};

class BreadthFirstIterator : public std::iterator<std::forward_iterator_tag, unsigned> {
  const TreeFragment* tf_;
  std::deque<TFIState> q_;
  unsigned sym;
 public:
  BreadthFirstIterator() : tf_(), sym() {}
  // used for begin
  explicit BreadthFirstIterator(const TreeFragment* tf, unsigned node_idx) : tf_(tf) {
    q_.push_back(TFIState(node_idx, 0, 0));
    Stage();
  }
  // used for end
  explicit BreadthFirstIterator(const TreeFragment* tf) : tf_(tf) {}
  const unsigned& operator*() const { return sym; }
  const unsigned* operator->() const { return &sym; }
  bool operator==(const BreadthFirstIterator& other) const {
    return (tf_ == other.tf_) && (q_ == other.q_);
  }
  bool operator!=(const BreadthFirstIterator& other) const {
    return (tf_ != other.tf_) || (q_ != other.q_);
  }
  const BreadthFirstIterator& operator++() {
    TFIState& s = q_.front();
    if (s.state == 0) {
      s.state++;
      Stage();
    } else {
      const unsigned len = tf_->nodes[s.node].rhs.size();
      s.rhspos++;
      if (s.rhspos >= len) {
        q_.pop_front();
        Stage();
      } else {
        Stage();
      }
    }
    return *this;
  }
  BreadthFirstIterator operator++(int) {
    BreadthFirstIterator res = *this;
    ++(*this);
    return res;
  }
  // tell iterator not to explore the subtree rooted at sym
  // should only be called once per NT symbol encountered
  const BreadthFirstIterator& truncate() {
    assert(IsRHS(sym));
    sym &= ALL_MASK;
    sym |= FRONTIER_BIT;
    q_.pop_back();
    return *this;
  }
  BreadthFirstIterator remainder() const {
    assert(IsRHS(sym));
    return BreadthFirstIterator(tf_, q_.back());
  }
  bool at_end() const {
    return q_.empty();
  }
 private:
  void Stage() {
    if (q_.empty()) return;
    const TFIState& s = q_.front();
    if (s.state == 0) {
      sym = (tf_->nodes[s.node].lhs & ALL_MASK) | LHS_BIT;
    } else {
      sym = tf_->nodes[s.node].rhs[s.rhspos];
      if (IsRHS(sym)) {
        q_.push_back(TFIState(sym & ALL_MASK, 0, 0));
        sym = tf_->nodes[sym & ALL_MASK].lhs | RHS_BIT;
      }
    }
  }

  // used by remainder
  BreadthFirstIterator(const TreeFragment* tf, const TFIState& s) : tf_(tf) {
    q_.push_back(s);
    Stage();
  }
};

inline std::ostream& operator<<(std::ostream& os, const TreeFragment& x) {
  x.DebugRec(x.nodes.size() - 1, &os);
  return os;
}

}

#endif
