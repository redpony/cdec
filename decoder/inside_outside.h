#ifndef _INSIDE_H_
#define _INSIDE_H_

#include <vector>
#include <algorithm>
#include "hg.h"

// run the inside algorithm and return the inside score
// if result is non-NULL, result will contain the inside
// score for each node
// NOTE: WeightType(0) must construct the semiring's additive identity
//       WeightType(1) must construct the semiring's multiplicative identity
template<typename WeightType, typename WeightFunction>
WeightType Inside(const Hypergraph& hg,
                  std::vector<WeightType>* result = NULL,
                  const WeightFunction& weight = WeightFunction()) {
  const int num_nodes = hg.nodes_.size();
  std::vector<WeightType> dummy;
  std::vector<WeightType>& inside_score = result ? *result : dummy;
  inside_score.resize(num_nodes);
  std::fill(inside_score.begin(), inside_score.end(), WeightType());
  for (int i = 0; i < num_nodes; ++i) {
    const Hypergraph::Node& cur_node = hg.nodes_[i];
    WeightType* const cur_node_inside_score = &inside_score[i];
    const int num_in_edges = cur_node.in_edges_.size();
    if (num_in_edges == 0) {
      *cur_node_inside_score = WeightType(1);
      continue;
    }
    for (int j = 0; j < num_in_edges; ++j) {
      const Hypergraph::Edge& edge = hg.edges_[cur_node.in_edges_[j]];
      WeightType score = weight(edge);
      for (int k = 0; k < edge.tail_nodes_.size(); ++k) {
        const int tail_node_index = edge.tail_nodes_[k];
        score *= inside_score[tail_node_index];
      }
      *cur_node_inside_score += score;
    }
  }
  return inside_score.back();
}

template<typename WeightType, typename WeightFunction>
void Outside(const Hypergraph& hg,
             std::vector<WeightType>& inside_score,
             std::vector<WeightType>* result,
             const WeightFunction& weight = WeightFunction()) {
  assert(result);
  const int num_nodes = hg.nodes_.size();
  assert(inside_score.size() == num_nodes);
  std::vector<WeightType>& outside_score = *result;
  outside_score.resize(num_nodes);
  std::fill(outside_score.begin(), outside_score.end(), WeightType(0));
  outside_score.back() = WeightType(1);
  for (int i = num_nodes - 1; i >= 0; --i) {
    const Hypergraph::Node& cur_node = hg.nodes_[i];
    const WeightType& head_node_outside_score = outside_score[i];
    const int num_in_edges = cur_node.in_edges_.size();
    for (int j = 0; j < num_in_edges; ++j) {
      const Hypergraph::Edge& edge = hg.edges_[cur_node.in_edges_[j]];
      const WeightType head_and_edge_weight = weight(edge) * head_node_outside_score;
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
        *tail_outside_score += head_and_edge_weight * inside_contribution;
      }
    }
  }
}

// this is the Inside-Outside optimization described in Li et al. (EMNLP 2009)
// for computing the inside algorithm over expensive semirings
// (such as expectations over features).  See Figure 4.  It is slightly different
// in that x/k is returned not (k,x)
// NOTE: RType * PType must be valid (and yield RType)
template<typename PType, typename WeightFunction, typename RType, typename WeightFunction2>
PType InsideOutside(const Hypergraph& hg,
                    RType* result_x,
                    const WeightFunction& weight1 = WeightFunction(),
                    const WeightFunction2& weight2 = WeightFunction2()) {
  const int num_nodes = hg.nodes_.size();
  std::vector<PType> inside, outside;
  const PType z = Inside<PType,WeightFunction>(hg, &inside, weight1);
  Outside<PType,WeightFunction>(hg, inside, &outside, weight1);
  RType& x = *result_x;
  x = RType();
  for (int i = 0; i < num_nodes; ++i) {
    const Hypergraph::Node& cur_node = hg.nodes_[i];
    const int num_in_edges = cur_node.in_edges_.size();
    for (int j = 0; j < num_in_edges; ++j) {
      const Hypergraph::Edge& edge = hg.edges_[cur_node.in_edges_[j]];
      PType prob = outside[i];
      prob *= weight1(edge);
      const int num_tail_nodes = edge.tail_nodes_.size();
      for (int k = 0; k < num_tail_nodes; ++k)
        prob *= inside[edge.tail_nodes_[k]];
      prob /= z;
      x += weight2(edge) * prob;
    }
  }
  return z;
}

#endif
