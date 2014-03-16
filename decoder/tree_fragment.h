#ifndef TREE_FRAGMENT
#define TREE_FRAGMENT

#include <iostream>
#include <vector>
#include <string>

#include "tdict.h"

namespace cdec {

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

inline std::ostream& operator<<(std::ostream& os, const TreeFragment& x) {
  x.DebugRec(x.nodes.size() - 1, &os);
  return os;
}

}

#endif
