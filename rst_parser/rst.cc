#include "rst.h"

using namespace std;

// David B. Wilson. Generating Random Spanning Trees More Quickly than the Cover Time.
// this is an awesome algorithm
TreeSampler::TreeSampler(const ArcFactoredForest& af) : forest(af), usucc(af.size() + 1) {
  // edges are directed from modifiers to heads, and finally to the root
  vector<double> p;
  for (int m = 1; m <= forest.size(); ++m) {
#if USE_ALIAS_SAMPLER
    p.clear();
#else
    SampleSet<double>& ss = usucc[m];
#endif
    double z = 0;
    for (int h = 0; h <= forest.size(); ++h) {
      double u = forest(h-1,m-1).edge_prob.as_float();
      z += u;
#if USE_ALIAS_SAMPLER
      p.push_back(u);
#else
      ss.add(u);
#endif
    }
#if USE_ALIAS_SAMPLER
    for (int i = 0; i < p.size(); ++i) { p[i] /= z; }
    usucc[m].Init(p);
#endif
  }
}

void TreeSampler::SampleRandomSpanningTree(EdgeSubset* tree, MT19937* prng) {
  MT19937& rng = *prng;
  const int r = 0;
  bool success = false;
  while (!success) {
    int roots = 0;
    tree->h_m_pairs.clear();
    tree->roots.clear();
    vector<int> next(forest.size() + 1, -1);
    vector<char> in_tree(forest.size() + 1, 0);
    in_tree[r] = 1;
    //cerr << "Forest size: " << forest.size() << endl;
    for (int i = 0; i <= forest.size(); ++i) {
      //cerr << "Sampling starting at u=" << i << endl;
      int u = i;
      if (in_tree[u]) continue;
      while(!in_tree[u]) {
#if USE_ALIAS_SAMPLER
        next[u] = usucc[u].Draw(rng);
#else
        next[u] = rng.SelectSample(usucc[u]);
#endif
        u = next[u];
      }
      u = i;
      //cerr << (u-1);
      int prev = u-1;
      while(!in_tree[u]) {
        in_tree[u] = true;
        u = next[u];
        //cerr << " > " << (u-1);
        if (u == r) {
          ++roots;
          tree->roots.push_back(prev);
        } else {
          tree->h_m_pairs.push_back(make_pair<short,short>(u-1,prev));
        }
        prev = u-1;
      }
      //cerr << endl;
    }
    assert(roots > 0);
    if (roots > 1) {
      //cerr << "FAILURE\n";
    } else {
      success = true;
    }
  }
};

