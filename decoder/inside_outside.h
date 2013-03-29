#ifndef _INSIDE_OUTSIDE_H_
#define _INSIDE_OUTSIDE_H_

#include <vector>
#include <algorithm>
#include "hg.h"

// semiring for Inside/Outside
struct Boolean {
  bool x;
  Boolean() : x() {  }
  Boolean(bool i) : x(i) {  }
  operator bool() const { return x; } // careful - this might cause a disaster with (bool)a + Boolean(b).
  // normally you'd use the logical (short circuit) || &&  operators, but bool really is guaranteed to be 0 or 1 numerically.  also note that | and & have equal precedence (!)
  void operator+=(Boolean o) { x|=o.x; }
  friend inline Boolean operator +(Boolean a,Boolean b) {
    return Boolean(a.x|b.x);
  }
  void operator*=(Boolean o) { x&=o.x; }
  friend inline Boolean operator *(Boolean a,Boolean b) {
    return Boolean(a.x&b.x);
  }
};

// run the inside algorithm and return the inside score
// if result is non-NULL, result will contain the inside
// score for each node
// NOTE: WeightType()  must construct the semiring's additive identity
//       WeightType(1) must construct the semiring's multiplicative identity
template<class WeightType, class WeightFunction>
WeightType Inside(const Hypergraph& hg,
                  std::vector<WeightType>* result = NULL,
                  const WeightFunction& weight = WeightFunction()) {
  const unsigned num_nodes = hg.nodes_.size();
  std::vector<WeightType> dummy;
  std::vector<WeightType>& inside_score = result ? *result : dummy;
  inside_score.clear();
  inside_score.resize(num_nodes);
//  std::fill(inside_score.begin(), inside_score.end(), WeightType()); // clear handles
  for (unsigned i = 0; i < num_nodes; ++i) {
    WeightType* const cur_node_inside_score = &inside_score[i];
    Hypergraph::EdgesVector const& in=hg.nodes_[i].in_edges_;
    const unsigned num_in_edges = in.size();
    for (unsigned j = 0; j < num_in_edges; ++j) {
      const HG::Edge& edge = hg.edges_[in[j]];
      WeightType score = weight(edge);
      for (unsigned k = 0; k < edge.tail_nodes_.size(); ++k) {
        const int tail_node_index = edge.tail_nodes_[k];
        score *= inside_score[tail_node_index];
      }
      *cur_node_inside_score += score;
    }
  }
  return inside_score.empty() ? WeightType(0) : inside_score.back();
}

template<class WeightType, class WeightFunction>
void Outside(const Hypergraph& hg,
             std::vector<WeightType>& inside_score,
             std::vector<WeightType>* result,
             const WeightFunction& weight = WeightFunction(),
             WeightType scale_outside = WeightType(1)
  ) {
  assert(result);
  const int num_nodes = hg.nodes_.size();
  assert(static_cast<int>(inside_score.size()) == num_nodes);
  std::vector<WeightType>& outside_score = *result;
  outside_score.clear();
  outside_score.resize(num_nodes);
//  std::fill(outside_score.begin(), outside_score.end(), WeightType()); // cleared
  outside_score.back() = scale_outside;
  for (int i = num_nodes - 1; i >= 0; --i) {
    const WeightType& head_node_outside_score = outside_score[i];
    Hypergraph::EdgesVector const& in=hg.nodes_[i].in_edges_;
    const int num_in_edges = in.size();
    for (int j = 0; j < num_in_edges; ++j) {
      const HG::Edge& edge = hg.edges_[in[j]];
      WeightType head_and_edge_weight = weight(edge);
      head_and_edge_weight *= head_node_outside_score;
      const int num_tail_nodes = edge.tail_nodes_.size();
      for (int k = 0; k < num_tail_nodes; ++k) {
        const int update_tail_node_index = edge.tail_nodes_[k];
        WeightType* const tail_outside_score = &outside_score[update_tail_node_index];
        WeightType inside_contribution = WeightType(1);
        for (int l = 0; l < num_tail_nodes; ++l) {
          const int other_tail_node_index = edge.tail_nodes_[l];
          if (update_tail_node_index != other_tail_node_index)
            inside_contribution *= inside_score[other_tail_node_index];
        }
        inside_contribution *= head_and_edge_weight;
        *tail_outside_score += inside_contribution;
      }
    }
  }
}

template <class K> // obviously not all semirings have a multiplicative inverse
struct OutsideNormalize {
  bool enable;
  OutsideNormalize(bool enable=true) : enable(enable) {}
  K operator()(K k) { return enable?K(1)/k:K(1); }
};
template <class K>
struct Outside1 {
  K operator()(K) { return K(1); }
};

template <class KType>
struct InsideOutsides {
//  typedef typename KWeightFunction::Weight KType;
  typedef std::vector<KType> Ks;
  Ks inside,outside;
  KType root_inside() {
    return inside.back();
  }
  InsideOutsides() {  }
  template <class KWeightFunction>
  KType compute(Hypergraph const& hg,KWeightFunction const& kwf=KWeightFunction()) {
    return compute(hg,Outside1<KType>(),kwf);
  }

  template <class KWeightFunction,class O1>
  KType compute(Hypergraph const& hg,O1 outside1,KWeightFunction const& kwf=KWeightFunction()) {
    typedef typename KWeightFunction::Weight KType2;
    assert(sizeof(KType2)==sizeof(KType)); // why am I doing this?  because I want to share the vectors used for tropical and prob_t semirings.  should instead have separate value type from semiring operations?  or suck it up and split the code calling in Prune* into 2 types (template)
    typedef std::vector<KType2> K2s;
    K2s &inside2=reinterpret_cast<K2s &>(inside);
    Inside<KType2,KWeightFunction>(hg, &inside2, kwf);
    KType scale=outside1(reinterpret_cast<KType const&>(inside2.back()));
    Outside<KType2,KWeightFunction>(hg, inside2, reinterpret_cast<K2s *>(&outside), kwf, reinterpret_cast<KType2 const&>(scale));
    return root_inside();
  }
// XWeightFunction::Result is result
  template <class XWeightFunction>
  typename XWeightFunction::Result expect(Hypergraph const& hg,XWeightFunction const& xwf=XWeightFunction())  {
    typename XWeightFunction::Result x;      // default constructor is semiring 0
    for (int i = 0,num_nodes=hg.nodes_.size(); i < num_nodes; ++i) {
      Hypergraph::EdgesVector const& in=hg.nodes_[i].in_edges_;
      const int num_in_edges = in.size();
      for (int j = 0; j < num_in_edges; ++j) {
        const HG::Edge& edge = hg.edges_[in[j]];
        KType kbar_e = outside[i];
        const int num_tail_nodes = edge.tail_nodes_.size();
        for (int k = 0; k < num_tail_nodes; ++k)
          kbar_e *= inside[edge.tail_nodes_[k]];
        x += xwf(edge) * kbar_e;
      }
    }
    return x;
  }
  template <class V,class VWeight>
  void compute_edge_marginals(Hypergraph const& hg,std::vector<V> &vs,VWeight const& weight) {
    vs.resize(hg.edges_.size());
    for (int i = 0,num_nodes=hg.nodes_.size(); i < num_nodes; ++i) {
      Hypergraph::EdgesVector const& in=hg.nodes_[i].in_edges_;
      const int num_in_edges = in.size();
      for (int j = 0; j < num_in_edges; ++j) {
        int edgei=in[j];
        const HG::Edge& edge = hg.edges_[edgei];
        V x=weight(edge)*outside[i];
        const int num_tail_nodes = edge.tail_nodes_.size();
        for (int k = 0; k < num_tail_nodes; ++k)
          x *= inside[edge.tail_nodes_[k]];
        vs[edgei] = x;
      }
    }
  }

};


// this is the Inside-Outside optimization described in Li and Eisner (EMNLP 2009)
// for computing the inside algorithm over expensive semirings
// (such as expectations over features).  See Figure 4.
// NOTE: XType * KType must be valid (and yield XType)
// NOTE: This may do things slightly differently than you are used to, please
// read the description in Li and Eisner (2009) carefully!
template<class KType, class KWeightFunction, class XType, class XWeightFunction>
KType InsideOutside(const Hypergraph& hg,
                    XType* result_x,
                    const KWeightFunction& kwf = KWeightFunction(),
                    const XWeightFunction& xwf = XWeightFunction()) {
  InsideOutsides<KType> io;
  io.compute(hg,kwf);
  *result_x=io.expect(hg,xwf);
  return io.root_inside();
}

#endif
