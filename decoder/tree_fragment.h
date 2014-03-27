#ifndef TREE_FRAGMENT
#define TREE_FRAGMENT

#include <queue>
#include <iostream>
#include <vector>
#include <string>

#include "tdict.h"

namespace cdec {

class BreadthFirstIterator;

static const unsigned NT_BIT       = 0x40000000u;
static const unsigned FRONTIER_BIT = 0x80000000u;
static const unsigned ALL_MASK     = 0x0FFFFFFFu;

inline bool IsInternalNT(unsigned x) {
  return (x & NT_BIT);
}

inline bool IsFrontier(unsigned x) {
  return (x & FRONTIER_BIT);
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
  TFIState() : node(), rhspos() {}
  TFIState(unsigned n, unsigned p) : node(n), rhspos(p) {}
  bool operator==(const TFIState& o) const { return node == o.node && rhspos == o.rhspos; }
  bool operator!=(const TFIState& o) const { return node != o.node && rhspos != o.rhspos; }
  unsigned short node;
  unsigned short rhspos;
};

class BreadthFirstIterator : public std::iterator<std::forward_iterator_tag, unsigned> {
  const TreeFragment* tf_;
  std::queue<TFIState> q_;
  unsigned sym;
 public:
  explicit BreadthFirstIterator(const TreeFragment* tf) : tf_(tf) {
    q_.push(TFIState(tf->nodes.size() - 1, 0));
    Stage();
  }
  BreadthFirstIterator(const TreeFragment* tf, int) : tf_(tf) {}
  const unsigned& operator*() const { return sym; }
  const unsigned* operator->() const { return &sym; }
  bool operator==(const BreadthFirstIterator& other) const {
    return (tf_ == other.tf_) && (q_ == other.q_);
  }
  bool operator!=(const BreadthFirstIterator& other) const {
    return (tf_ != other.tf_) || (q_ != other.q_);
  }
  void Stage() {
    if (q_.empty()) return;
    const TFIState& s = q_.front();
    sym = tf_->nodes[s.node].rhs[s.rhspos];
    if (IsInternalNT(sym)) {
      q_.push(TFIState(sym & ALL_MASK, 0));
      sym = tf_->nodes[sym & ALL_MASK].lhs;
    }
  }
  const BreadthFirstIterator& operator++() {
    TFIState& s = q_.front();
    const unsigned len = tf_->nodes[s.node].rhs.size();
    s.rhspos++;
    if (s.rhspos > len) {
      q_.pop();
      Stage();
    } else if (s.rhspos == len) {
      sym = 0;
    } else {
      Stage();
    }
    return *this;
  }
  BreadthFirstIterator operator++(int) {
    BreadthFirstIterator res = *this;
    ++(*this);
    return res;
  }
};

inline std::ostream& operator<<(std::ostream& os, const TreeFragment& x) {
  x.DebugRec(x.nodes.size() - 1, &os);
  return os;
}

}

#endif
