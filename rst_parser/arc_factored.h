#ifndef _ARC_FACTORED_H_
#define _ARC_FACTORED_H_

#include <iostream>
#include <cassert>
#include <vector>
#include <utility>
#include "array2d.h"
#include "sparse_vector.h"
#include "prob.h"
#include "weights.h"

struct EdgeSubset {
  EdgeSubset() {}
  std::vector<short> roots; // unless multiroot trees are supported, this
                            // will have a single member
  std::vector<std::pair<short, short> > h_m_pairs; // h,m start at *1*
};

class ArcFactoredForest {
 public:
  explicit ArcFactoredForest(short num_words) :
      num_words_(num_words),
      root_edges_(num_words),
      edges_(num_words, num_words) {
    for (int h = 0; h < num_words; ++h) {
      for (int m = 0; m < num_words; ++m) {
        edges_(h, m).h = h + 1;
        edges_(h, m).m = m + 1;
      }
      root_edges_[h].h = 0;
      root_edges_[h].m = h + 1;
    }
  }

  // compute the maximum spanning tree based on the current weighting
  // using the O(n^2) CLE algorithm
  void MaximumEdgeSubset(EdgeSubset* st) const;

  // Reweight edges so that edge_prob is the edge's marginals
  // optionally returns log partition
  void EdgeMarginals(double* p_log_z = NULL);

  // This may not return a tree
  void PickBestParentForEachWord(EdgeSubset* st) const;

  struct Edge {
    Edge() : h(), m(), features(), edge_prob(prob_t::Zero()) {}
    short h;
    short m;
    SparseVector<weight_t> features;
    prob_t edge_prob;
  };

  const Edge& operator()(short h, short m) const {
    assert(m > 0);
    assert(m <= num_words_);
    assert(h >= 0);
    assert(h <= num_words_);
    return h ? edges_(h - 1, m - 1) : root_edges_[m - 1];
  }

  Edge& operator()(short h, short m) {
    assert(m > 0);
    assert(m <= num_words_);
    assert(h >= 0);
    assert(h <= num_words_);
    return h ? edges_(h - 1, m - 1) : root_edges_[m - 1];
  }

  float Weight(short h, short m) const {
    return log((*this)(h,m).edge_prob);
  }

  template <class V>
  void Reweight(const V& weights) {
    for (int m = 0; m < num_words_; ++m) {
      for (int h = 0; h < num_words_; ++h) {
        if (h != m) {
          Edge& e = edges_(h, m);
          e.edge_prob.logeq(e.features.dot(weights));
        }
      }
      Edge& e = root_edges_[m];
      e.edge_prob.logeq(e.features.dot(weights));
    }
  }

 private:
  unsigned num_words_;
  std::vector<Edge> root_edges_;
  Array2D<Edge> edges_;
};

inline std::ostream& operator<<(std::ostream& os, const ArcFactoredForest::Edge& edge) {
  return os << "(" << edge.h << " < " << edge.m << ")";
}

inline std::ostream& operator<<(std::ostream& os, const EdgeSubset& ss) {
  for (unsigned i = 0; i < ss.roots.size(); ++i)
    os << "ROOT < " << ss.roots[i] << std::endl;
  for (unsigned i = 0; i < ss.h_m_pairs.size(); ++i)
    os << ss.h_m_pairs[i].first << " < " << ss.h_m_pairs[i].second << std::endl;
  return os;
}

#endif
