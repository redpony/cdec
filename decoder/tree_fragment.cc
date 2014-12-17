#include "tree_fragment.h"

#include <cassert>

#include "tdict.h"

using namespace std;

namespace cdec {

TreeFragment::TreeFragment(const StringPiece& tree, bool allow_frontier_sites) {
  int bal = 0;
  const unsigned len = tree.size();
  unsigned cur = 0;
  unsigned open = 0, close = 0;
  for (auto& c : tree) {
    ++cur;
    if (c == '(') { ++open; ++bal; }
    else if (c == ')') {
      ++close; --bal;
      if (bal < 1 && cur != len) {
        cerr << "Badly formed tree detected at column " << cur << " in:\n" << tree << endl;
        abort();
      }
    }
  }
  nodes.resize(open);
  unsigned cp = 0, symp = 0, np = 0;
  ParseRec(tree, allow_frontier_sites, cp, symp, np, &cp, &symp, &np);
  root = nodes.back().lhs;
  if (!allow_frontier_sites) SetupSpansRec(open - 1, 0);
  //cerr << "ROOT: " << TD::Convert(root & ALL_MASK) << endl;
  //DebugRec(open - 1, &cerr); cerr << "\n";
}

void TreeFragment::DebugRec(unsigned cur, ostream* out) const {
  *out << '(' << TD::Convert(nodes[cur].lhs & ALL_MASK);
  // *out << "_{" << nodes[cur].span.first << ',' << nodes[cur].span.second << '}';
  for (auto& x : nodes[cur].rhs) {
    *out << ' ';
    if (IsFrontier(x)) {
      *out << '[' << TD::Convert(x & ALL_MASK) << ']';
    } else if (IsRHS(x)) {
      DebugRec(x & ALL_MASK, out);
    } else { // must be terminal
      *out << TD::Convert(x);
    }
  }
  *out << ')';
}

// returns left + the number of terminals rooted at NT cur,
int TreeFragment::SetupSpansRec(unsigned cur, int left) {
  int right = left;
  for (auto& x : nodes[cur].rhs) {
    if (IsRHS(x)) {
      right = SetupSpansRec(x & ALL_MASK, right);
    } else {
      ++right;
    }
  }
  nodes[cur].span.first = left;
  nodes[cur].span.second = right;
  return right;
}

vector<int> TreeFragment::Terminals() const {
  vector<int> terms;
  for (auto& x : *this)
    if (IsTerminal(x)) terms.push_back(x);
  return terms;
}

// cp is the character index in the tree
// np keeps track of the nodes (nonterminals) that have been built
// symp keeps track of the terminal symbols that have been built
void TreeFragment::ParseRec(const StringPiece& tree, bool afs, unsigned cp, unsigned symp, unsigned np, unsigned* pcp, unsigned* psymp, unsigned* pnp) {
  if (tree[cp] != '(') {
    cerr << "Expected ( at " << cp << endl;
    abort();
  }
  const unsigned i = symp;
  vector<unsigned> rhs; // w | 0 = terminal, w | NT_BIT, index | FRONTIER_BIT
  ++cp;
  while(tree[cp] == ' ') { ++cp; }
  const unsigned nt_start = cp;
  while(tree[cp] != ' ' && tree[cp] != '(' && tree[cp] != ')') { ++cp; }
  const unsigned nt_end = cp;
  while(tree[cp] == ' ') { ++cp; }
  while (tree[cp] != ')') {
    if (tree[cp] == '(') {
      // recursively call parser to deal with constituent
      ParseRec(tree, afs, cp, symp, np, &cp, &symp, &np);
      unsigned ind = np - 1;
      rhs.push_back(ind | RHS_BIT);
    } else { // deal with terminal / nonterminal substitution
      ++symp;
      assert(tree[cp] != ' ');
      const unsigned t_start = cp;
      while(tree[cp] != ' ' && tree[cp] != ')' && tree[cp] != '(') { ++cp; }
      const unsigned t_end = cp;
      while(tree[cp] == ' ') { ++cp; }
      // TODO: add a terminal symbol to the current edge
      const bool is_terminal = tree[t_start] != '[' || (t_end - t_start < 3 || tree[t_end - 1] != ']');
      if (is_terminal) {
        const unsigned term = TD::Convert(tree.substr(t_start, t_end - t_start).as_string());
        rhs.push_back(term);
        // cerr << "T='" << TD::Convert(term) << "'\n";
        ++terminals;
      } else { // frontier site (NT but no recursion)
        const unsigned nt = TD::Convert(tree.substr(t_start + 1, t_end - t_start - 2).as_string()) | FRONTIER_BIT;
        rhs.push_back(nt);
        ++frontier_sites;
        // cerr << "FRONT-NT=[" << TD::Convert(nt & ALL_MASK) << "]\n";
        if (!afs) {
          cerr << "Frontier sites not allowed in input: " << tree << endl;
          abort();
        }
      } 
    }
  } // continuent has completed, cp is at ), build node
  const unsigned j = symp; // span from (i,j)
  // add an internal non-terminal symbol
  const unsigned nt = TD::Convert(tree.substr(nt_start, nt_end - nt_start).as_string()) | RHS_BIT;
  nodes[np] = TreeFragmentProduction(nt, rhs);
  //cerr << np << " production(" << i << "," << j << ")=  " << TD::Convert(nt & ALL_MASK) << " -->";
  //for (auto& x : rhs) {
  //  cerr << ' ';
  //  if (IsFrontier(x)) cerr << '*';
  //  if (IsInternalNT(x)) cerr << TD::Convert(nodes[x & ALL_MASK].lhs & ALL_MASK); else
  //    cerr << TD::Convert(x & ALL_MASK);
  //}
  //cerr << "\n   "; DebugRec(np,&cerr); cerr << endl;
  ++cp;
  while(tree[cp] == ' ' && cp < tree.size()) { ++cp; }
  *pcp = cp;
  *pnp = np + 1;
  *psymp = symp;
}

}
