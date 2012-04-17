#include "arc_factored.h"

#include <set>
#include <tr1/unordered_set>

#include <boost/pending/disjoint_sets.hpp>
#include <boost/functional/hash.hpp>

#include "arc_ff.h"

using namespace std;
using namespace std::tr1;
using namespace boost;

void EdgeSubset::ExtractFeatures(const TaggedSentence& sentence,
                                 const ArcFeatureFunctions& ffs,
                                 SparseVector<double>* features) const {
  SparseVector<weight_t> efmap;
  for (int j = 0; j < h_m_pairs.size(); ++j) {
    efmap.clear();
    ffs.EdgeFeatures(sentence, h_m_pairs[j].first,
                     h_m_pairs[j].second,
                     &efmap);
    (*features) += efmap;
  }
  for (int j = 0; j < roots.size(); ++j) {
    efmap.clear();
    ffs.EdgeFeatures(sentence, -1, roots[j], &efmap);
    (*features) += efmap;
  }
}

void ArcFactoredForest::ExtractFeatures(const TaggedSentence& sentence,
                                        const ArcFeatureFunctions& ffs) {
  for (int m = 0; m < num_words_; ++m) {
    for (int h = 0; h < num_words_; ++h) {
      ffs.EdgeFeatures(sentence, h, m, &edges_(h,m).features);
    }
    ffs.EdgeFeatures(sentence, -1, m, &root_edges_[m].features);
  }
}

void ArcFactoredForest::PickBestParentForEachWord(EdgeSubset* st) const {
  for (int m = 0; m < num_words_; ++m) {
    int best_head = -2;
    prob_t best_score;
    for (int h = -1; h < num_words_; ++h) {
      const Edge& edge = (*this)(h,m);
      if (best_head < -1 || edge.edge_prob > best_score) {
        best_score = edge.edge_prob;
        best_head = h;
      }
    }
    assert(best_head >= -1);
    if (best_head >= 0)
      st->h_m_pairs.push_back(make_pair<short,short>(best_head, m));
    else
      st->roots.push_back(m);
  }
}

struct WeightedEdge {
  WeightedEdge() : h(), m(), weight() {}
  WeightedEdge(short hh, short mm, float w) : h(hh), m(mm), weight(w) {}
  short h, m;
  float weight;
  inline bool operator==(const WeightedEdge& o) const {
    return h == o.h && m == o.m && weight == o.weight;
  }
  inline bool operator!=(const WeightedEdge& o) const {
    return h != o.h || m != o.m || weight != o.weight;
  }
};
inline bool operator<(const WeightedEdge& l, const WeightedEdge& o) { return l.weight < o.weight; }
inline size_t hash_value(const WeightedEdge& e) { return reinterpret_cast<const size_t&>(e); }


struct PriorityQueue {
  void push(const WeightedEdge& e) {}
  const WeightedEdge& top() const {
    static WeightedEdge w(1,2,3);
    return w;
  }
  void pop() {}
  void increment_all(float p) {}
};

// based on Trajan 1977
void ArcFactoredForest::MaximumSpanningTree(EdgeSubset* st) const {
  typedef disjoint_sets_with_storage<identity_property_map, identity_property_map,
      find_with_full_path_compression> DisjointSet;
  DisjointSet strongly(num_words_ + 1);
  DisjointSet weakly(num_words_ + 1);
  set<unsigned> roots, rset;
  unordered_set<WeightedEdge, boost::hash<WeightedEdge> > h;
  vector<PriorityQueue> qs(num_words_ + 1);
  vector<WeightedEdge> enter(num_words_ + 1);
  vector<unsigned> mins(num_words_ + 1);
  const WeightedEdge kDUMMY(0,0,0.0f);
  for (unsigned i = 0; i <= num_words_; ++i) {
    if (i > 0) {
      // I(i) incidence on i -- all incoming edges
      for (unsigned j = 0; j <= num_words_; ++j) {
        qs[i].push(WeightedEdge(j, i, Weight(j,i)));
      }
    }
    strongly.make_set(i);
    weakly.make_set(i);
    roots.insert(i);
    enter[i] = kDUMMY;
    mins[i] = i;
  }
  while(!roots.empty()) {
    set<unsigned>::iterator it = roots.begin();
    const unsigned k = *it;
    roots.erase(it);
    cerr << "k=" << k << endl;
    WeightedEdge ij = qs[k].top();  // MAX(k)
    qs[k].pop();
    if (ij.weight <= 0) {
      rset.insert(k);
    } else {
      if (strongly.find_set(ij.h) == k) {
        roots.insert(k);
      } else {
        h.insert(ij);
        if (weakly.find_set(ij.h) != weakly.find_set(ij.m)) {
          weakly.union_set(ij.h, ij.m);
          enter[k] = ij;
        } else {
          unsigned vertex = 0;
          float val = 99999999999;
          WeightedEdge xy = ij;
          while(xy != kDUMMY) {
            if (xy.weight < val) {
              val = xy.weight;
              vertex = strongly.find_set(xy.m);
            }
            xy = enter[strongly.find_set(xy.h)];
          }
          qs[k].increment_all(val - ij.weight);
          mins[k] = mins[vertex];
          xy = enter[strongly.find_set(ij.h)];
          while (xy != kDUMMY) {
          }
        }
      }
    }
  }
}

