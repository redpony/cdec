#include "rst.h"

using namespace std;

// David B. Wilson. Generating Random Spanning Trees More Quickly than the Cover Time.

TreeSampler::TreeSampler(const ArcFactoredForest& af) : forest(af), usucc(af.size() + 1) {
  // edges are directed from modifiers to heads, to the root
  for (int m = 1; m <= forest.size(); ++m) {
    SampleSet<double>& ss = usucc[m];
    for (int h = 0; h <= forest.size(); ++h)
      ss.add(forest(h-1,m-1).edge_prob.as_float());
  }
}

void TreeSampler::SampleRandomSpanningTree(EdgeSubset* tree) {
  MT19937 rng;
  const int r = 0;
  bool success = false;
  while (!success) {
    int roots = 0;
    vector<int> next(forest.size() + 1, -1);
    vector<char> in_tree(forest.size() + 1, 0);
    in_tree[r] = 1;
    for (int i = 0; i < forest.size(); ++i) {
      int u = i;
      if (in_tree[u]) continue;
      while(!in_tree[u]) {
        next[u] = rng.SelectSample(usucc[u]);
        u = next[u];
      }
      u = i;
      cerr << (u-1);
      while(!in_tree[u]) {
        in_tree[u] = true;
        u = next[u];
        cerr << " > " << (u-1);
        if (u == r) { ++roots; }
      }
      cerr << endl;
    }
    assert(roots > 0);
    if (roots > 1) {
      cerr << "FAILURE\n";
    } else {
      success = true;
    }
  }
};

