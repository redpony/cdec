#ifndef _ARC_FACTORED_H_
#define _ARC_FACTORED_H_

#include <vector>
#include <cassert>
#include "array2d.h"
#include "sparse_vector.h"

class ArcFactoredForest {
 public:
  explicit ArcFactoredForest(short num_words) :
      num_words_(num_words),
      root_edges_(num_words),
      edges_(num_words, num_words) {}

  struct Edge {
    Edge() : features(), edge_prob(prob_t::Zero()) {}
    SparseVector<weight_t> features;
    prob_t edge_prob;
  };

  template <class V>
  void Reweight(const V& weights) {
    for (int m = 0; m < num_words_; ++m) {
      for (int h = 0; h < num_words_; ++h) {
        if (h != m) {
          Edge& e = edges_(h, m);
          e.edge_prob.logeq(e.features.dot(weights));
        }
      }
      if (m) {
        Edge& e = root_edges_[m];
        e.edge_prob.logeq(e.features.dot(weights));
      }
    }
  }

  const Edge& operator()(short h, short m) const {
    assert(m > 0);
    assert(m <= num_words_);
    assert(h >= 0);
    assert(h <= num_words_);
    return h ? edges_(h - 1, m - 1) : root_edges[m - 1];
  }
  Edge& operator()(short h, short m) {
    assert(m > 0);
    assert(m <= num_words_);
    assert(h >= 0);
    assert(h <= num_words_);
    return h ? edges_(h - 1, m - 1) : root_edges[m - 1];
  }
 private:
  unsigned num_words_;
  std::vector<Edge> root_edges_;
  Array2D<Edge> edges_;
};

#endif
