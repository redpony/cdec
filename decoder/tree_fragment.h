#ifndef TREE_FRAGMENT
#define TREE_FRAGMENT

#include <deque>
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstddef>

#include "string_piece.hh"

namespace cdec {

class BreadthFirstIterator;
class DepthFirstIterator;

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
  TreeFragmentProduction(int nttype, const std::vector<unsigned>& r) : lhs(nttype), rhs(r), span(std::make_pair<short,short>(-1,-1)) {}
  unsigned lhs;
  std::vector<unsigned> rhs;
  std::pair<short, short> span; // the span of the node (in input, or not set for rules)
};

// this data structure represents a tree or forest
// productions can have mixtures of terminals and nonterminal symbols and non-terminal frontier sites
class TreeFragment {
 public:
  TreeFragment() : frontier_sites(), terminals() {}
  // (S (NP a (X b) c d) (VP (V foo) (NP (NN bar))))
  explicit TreeFragment(const StringPiece& tree, bool allow_frontier_sites = false);
  void DebugRec(unsigned cur, std::ostream* out) const;
  typedef DepthFirstIterator iterator;
  typedef ptrdiff_t difference_type;
  typedef unsigned value_type;
  typedef const unsigned * pointer;
  typedef const unsigned & reference;

  // default iterator is DFS
  iterator begin() const;
  iterator begin(unsigned node_idx) const;
  iterator end() const;

  BreadthFirstIterator bfs_begin() const;
  BreadthFirstIterator bfs_begin(unsigned node_idx) const;
  BreadthFirstIterator bfs_end() const;

 private:
  // cp is the character index in the tree
  // np keeps track of the nodes (nonterminals) that have been built
  // symp keeps track of the terminal symbols that have been built
  void ParseRec(const StringPiece& tree, bool afs, unsigned cp, unsigned symp, unsigned np, unsigned* pcp, unsigned* psymp, unsigned* pnp);

  // used by constructor to set up span indices for logging/alignment purposes
  int SetupSpansRec(unsigned cur, int left);

 public:
  unsigned root;
  unsigned char frontier_sites;
  unsigned short terminals;

  std::vector<TreeFragmentProduction> nodes;
};

struct TFIState {
  TFIState() : node(), rhspos(), state() {}
  TFIState(unsigned n, int p, unsigned s) : node(n), rhspos(p), state(s) {}
  bool operator==(const TFIState& o) const { return node == o.node && rhspos == o.rhspos && state == o.state; }
  bool operator!=(const TFIState& o) const { return node != o.node || rhspos != o.rhspos || state != o.state; }
  unsigned short node;
  short rhspos;
  unsigned char state;
};

class DepthFirstIterator : public std::iterator<std::forward_iterator_tag, unsigned> {
  const TreeFragment* tf_;
  std::deque<TFIState> q_;
  unsigned sym;
 public:
  DepthFirstIterator() : tf_(), sym() {}
  // used for begin
  explicit DepthFirstIterator(const TreeFragment* tf, unsigned node_idx) : tf_(tf) {
    q_.push_back(TFIState(node_idx, -1, 0));
    Stage();
    q_.back().state++;
  }
  // used for end
  explicit DepthFirstIterator(const TreeFragment* tf) : tf_(tf) {}
  const unsigned& operator*() const { return sym; }
  const unsigned* operator->() const { return &sym; }
  bool operator==(const DepthFirstIterator& other) const {
    return (tf_ == other.tf_) && (q_ == other.q_);
  }
  bool operator!=(const DepthFirstIterator& other) const {
    return (tf_ != other.tf_) || (q_ != other.q_);
  }
  unsigned node_idx() const { return q_.front().node; }
  const DepthFirstIterator& operator++() {
    TFIState& s = q_.back();
    if (s.state == 0) {
      Stage();
      s.state++;
    } else if (s.state == 1) {
      const unsigned len = tf_->nodes[s.node].rhs.size();
      s.rhspos++;
      if (s.rhspos >= len) {
        q_.pop_back();
        while (!q_.empty()) {
          TFIState& s = q_.back();
          const unsigned len = tf_->nodes[s.node].rhs.size();
          s.rhspos++;
          if (s.rhspos < len) break;
          q_.pop_back();
        }
      }
      Stage();
    }
    return *this;
  }
  DepthFirstIterator operator++(int) {
    DepthFirstIterator res = *this;
    ++(*this);
    return res;
  }
  // tell iterator not to explore the subtree rooted at sym
  // should only be called once per NT symbol encountered
  const DepthFirstIterator& truncate() {
    assert(IsRHS(sym));
    sym &= ALL_MASK;
    sym |= FRONTIER_BIT;
    q_.pop_back();
    return *this;
  }
  unsigned child_node() const {
    assert(IsRHS(sym));
    return q_.back().node;
  }
  DepthFirstIterator remainder() const {
    assert(IsRHS(sym));
    return DepthFirstIterator(tf_, q_.back());
  }
  bool at_end() const {
    return q_.empty();
  }
 private:
  void Stage() {
    if (q_.empty()) return;
    const TFIState& s = q_.back();
    if (s.state == 0) {
      sym = (tf_->nodes[s.node].lhs & ALL_MASK) | LHS_BIT;
    } else if (s.state == 1) {
      sym = tf_->nodes[s.node].rhs[s.rhspos];
      if (IsRHS(sym)) {
        q_.push_back(TFIState(sym & ALL_MASK, -1, 0));
        sym = tf_->nodes[sym & ALL_MASK].lhs | RHS_BIT;
      }
    }
  }

  // used by remainder
  DepthFirstIterator(const TreeFragment* tf, const TFIState& s) : tf_(tf) {
    q_.push_back(s);
    Stage();
  }
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
  unsigned node_idx() const { return q_.front().node; }
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
  unsigned child_node() const {
    assert(IsRHS(sym));
    return q_.back().node;
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

inline TreeFragment::iterator TreeFragment::begin() const { return iterator(this, nodes.size() - 1); }
inline TreeFragment::iterator TreeFragment::begin(unsigned node_idx) const { return iterator(this, node_idx); }
inline TreeFragment::iterator TreeFragment::end() const { return iterator(this); }

inline BreadthFirstIterator TreeFragment::bfs_begin() const { return BreadthFirstIterator(this, nodes.size() - 1); }
inline BreadthFirstIterator TreeFragment::bfs_begin(unsigned node_idx) const { return BreadthFirstIterator(this, node_idx); }
inline BreadthFirstIterator TreeFragment::bfs_end() const { return BreadthFirstIterator(this); }

inline std::ostream& operator<<(std::ostream& os, const TreeFragment& x) {
  x.DebugRec(x.nodes.size() - 1, &os);
  return os;
}

}

#endif
