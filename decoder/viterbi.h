#ifndef _VITERBI_H_
#define _VITERBI_H_

#include <vector>
#include "prob.h"
#include "hg.h"
#include "tdict.h"

// V must implement:
//  void operator()(const vector<const T*>& ants, T* result);
template<typename T, typename Traversal, typename WeightType, typename WeightFunction>
WeightType Viterbi(const Hypergraph& hg,
                   T* result,
                   const Traversal& traverse = Traversal(),
                   const WeightFunction& weight = WeightFunction()) {
  const int num_nodes = hg.nodes_.size();
  std::vector<T> vit_result(num_nodes);
  std::vector<WeightType> vit_weight(num_nodes, WeightType::Zero());

  for (int i = 0; i < num_nodes; ++i) {
    const Hypergraph::Node& cur_node = hg.nodes_[i];
    WeightType* const cur_node_best_weight = &vit_weight[i];
    T*          const cur_node_best_result = &vit_result[i];
    
    const int num_in_edges = cur_node.in_edges_.size();
    if (num_in_edges == 0) {
      *cur_node_best_weight = WeightType(1);
      continue;
    }
    for (int j = 0; j < num_in_edges; ++j) {
      const Hypergraph::Edge& edge = hg.edges_[cur_node.in_edges_[j]];
      WeightType score = weight(edge);
      std::vector<const T*> ants(edge.tail_nodes_.size());
      for (int k = 0; k < edge.tail_nodes_.size(); ++k) {
        const int tail_node_index = edge.tail_nodes_[k];
        score *= vit_weight[tail_node_index];
        ants[k] = &vit_result[tail_node_index];
      }
      if (*cur_node_best_weight < score) {
        *cur_node_best_weight = score;
        traverse(edge, ants, cur_node_best_result);
      }
    }
  }
  std::swap(*result, vit_result.back());
  return vit_weight.back();
}

struct PathLengthTraversal {
  void operator()(const Hypergraph::Edge& edge,
                  const std::vector<const int*>& ants,
                  int* result) const {
    (void) edge;
    *result = 1;
    for (int i = 0; i < ants.size(); ++i) *result += *ants[i];
  }
};

struct ESentenceTraversal {
  void operator()(const Hypergraph::Edge& edge,
                  const std::vector<const std::vector<WordID>*>& ants,
                  std::vector<WordID>* result) const {
    edge.rule_->ESubstitute(ants, result);
  }
};

struct ELengthTraversal {
  void operator()(const Hypergraph::Edge& edge,
                  const std::vector<const int*>& ants,
                  int* result) const {
    *result = edge.rule_->ELength() - edge.rule_->Arity();
    for (int i = 0; i < ants.size(); ++i) *result += *ants[i];
  }
};

struct FSentenceTraversal {
  void operator()(const Hypergraph::Edge& edge,
                  const std::vector<const std::vector<WordID>*>& ants,
                  std::vector<WordID>* result) const {
    edge.rule_->FSubstitute(ants, result);
  }
};

// create a strings of the form (S (X the man) (X said (X he (X would (X go)))))
struct ETreeTraversal {
  ETreeTraversal() : left("("), space(" "), right(")") {}
  const std::string left;
  const std::string space;
  const std::string right;
  void operator()(const Hypergraph::Edge& edge,
                  const std::vector<const std::vector<WordID>*>& ants,
                  std::vector<WordID>* result) const {
    std::vector<WordID> tmp;
    edge.rule_->ESubstitute(ants, &tmp);
    const std::string cat = TD::Convert(edge.rule_->GetLHS() * -1);
    if (cat == "Goal")
      result->swap(tmp);
    else
      TD::ConvertSentence(left + cat + space + TD::GetString(tmp) + right,
                          result);
  }
};

struct FTreeTraversal {
  FTreeTraversal() : left("("), space(" "), right(")") {}
  const std::string left;
  const std::string space;
  const std::string right;
  void operator()(const Hypergraph::Edge& edge,
                  const std::vector<const std::vector<WordID>*>& ants,
                  std::vector<WordID>* result) const {
    std::vector<WordID> tmp;
    edge.rule_->FSubstitute(ants, &tmp);
    const std::string cat = TD::Convert(edge.rule_->GetLHS() * -1);
    if (cat == "Goal")
      result->swap(tmp);
    else
      TD::ConvertSentence(left + cat + space + TD::GetString(tmp) + right,
                          result);
  }
};

struct ViterbiPathTraversal {
  void operator()(const Hypergraph::Edge& edge,
                  const std::vector<const std::vector<const Hypergraph::Edge*>* >& ants,
                  std::vector<const Hypergraph::Edge*>* result) const {
    result->clear();
    for (int i = 0; i < ants.size(); ++i)
      for (int j = 0; j < ants[i]->size(); ++j)
        result->push_back((*ants[i])[j]);
    result->push_back(&edge);
  }
};

prob_t ViterbiESentence(const Hypergraph& hg, std::vector<WordID>* result);
std::string ViterbiETree(const Hypergraph& hg);
prob_t ViterbiFSentence(const Hypergraph& hg, std::vector<WordID>* result);
std::string ViterbiFTree(const Hypergraph& hg);
int ViterbiELength(const Hypergraph& hg);
int ViterbiPathLength(const Hypergraph& hg);

#endif
