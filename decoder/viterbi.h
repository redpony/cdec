#ifndef _VITERBI_H_
#define _VITERBI_H_

#include <vector>
#include "prob.h"
#include "hg.h"
#include "tdict.h"
#include "filelib.h"
#include <boost/make_shared.hpp>

std::string viterbi_stats(Hypergraph const& hg, std::string const& name="forest", bool estring=true, bool etree=false, bool derivation_tree=false, bool extract_rules=false, boost::shared_ptr<WriteFile> extract_file = boost::make_shared<WriteFile>());

/// computes for each hg node the best (according to WeightType/WeightFunction) derivation, and some homomorphism (bottom up expression tree applied through Traversal) of it. T is the "return type" of Traversal, which is called only once for the best edge for a node's result (i.e. result will start default constructed)
//TODO: make T a typename inside Traversal and WeightType a typename inside WeightFunction?
// Traversal must implement:
//  typedef T Result;
//  void operator()(HG::Edge const& e,const vector<const Result*>& ants, Result* result) const;
// WeightFunction must implement:
//  typedef prob_t Weight;
//  Weight operator()(HG::Edge const& e) const;
template<class Traversal,class WeightFunction>
typename WeightFunction::Weight Viterbi(const Hypergraph& hg,
                   typename Traversal::Result* result,
                   const Traversal& traverse,
                   const WeightFunction& weight) {
  typedef typename Traversal::Result T;
  typedef typename WeightFunction::Weight WeightType;
  const int num_nodes = hg.nodes_.size();
  std::vector<T> vit_result(num_nodes);
  std::vector<WeightType> vit_weight(num_nodes, WeightType());

  for (int i = 0; i < num_nodes; ++i) {
    const Hypergraph::Node& cur_node = hg.nodes_[i];
    WeightType* const cur_node_best_weight = &vit_weight[i];
    T*          const cur_node_best_result = &vit_result[i];

    const unsigned num_in_edges = cur_node.in_edges_.size();
    if (num_in_edges == 0) {
      *cur_node_best_weight = WeightType(1);
      continue;
    }
    HG::Edge const* edge_best=0;
    for (unsigned j = 0; j < num_in_edges; ++j) {
      const HG::Edge& edge = hg.edges_[cur_node.in_edges_[j]];
      WeightType score = weight(edge);
      for (unsigned k = 0; k < edge.tail_nodes_.size(); ++k)
        score *= vit_weight[edge.tail_nodes_[k]];
      if (!edge_best || *cur_node_best_weight < score) {
        *cur_node_best_weight = score;
        edge_best=&edge;
      }
    }
    assert(edge_best);
    HG::Edge const& edgeb=*edge_best;
    std::vector<const T*> antsb(edgeb.tail_nodes_.size());
    for (unsigned k = 0; k < edgeb.tail_nodes_.size(); ++k)
      antsb[k] = &vit_result[edgeb.tail_nodes_[k]];
    traverse(edgeb, antsb, cur_node_best_result);
  }
  if (vit_result.empty())
    return WeightType(0);
  std::swap(*result, vit_result.back());
  return vit_weight.back();
}


/*
template<typename Traversal,typename WeightFunction>
typename WeightFunction::Weight Viterbi(const Hypergraph& hg,
                   typename Traversal::Result* result)
{
  Traversal traverse;
  WeightFunction weight;
  return Viterbi(hg,result,traverse,weight);
}

template<class Traversal,class WeightFunction=EdgeProb>
typename WeightFunction::Weight Viterbi(const Hypergraph& hg,
                   typename Traversal::Result* result,
                   Traversal const& traverse=Traversal()
  )
{
  WeightFunction weight;
  return Viterbi(hg,result,traverse,weight);
}
*/

//spec for EdgeProb
template<class Traversal>
prob_t Viterbi(const Hypergraph& hg,
                   typename Traversal::Result* result,
                   Traversal const& traverse=Traversal()
  )
{
  EdgeProb weight;
  return Viterbi(hg,result,traverse,weight);
}

struct PathLengthTraversal {
  typedef int Result;
  void operator()(const HG::Edge& edge,
                  const std::vector<const int*>& ants,
                  int* result) const {
    (void) edge;
    *result = 1;
    for (unsigned i = 0; i < ants.size(); ++i) *result += *ants[i];
  }
};

struct ESentenceTraversal {
  typedef std::vector<WordID> Result;
  void operator()(const HG::Edge& edge,
                  const std::vector<const Result*>& ants,
                  Result* result) const {
    edge.rule_->ESubstitute(ants, result);
  }
};

struct ELengthTraversal {
  typedef int Result;
  void operator()(const HG::Edge& edge,
                  const std::vector<const int*>& ants,
                  int* result) const {
    *result = edge.rule_->ELength() - edge.rule_->Arity();
    for (unsigned i = 0; i < ants.size(); ++i) *result += *ants[i];
  }
};

struct FSentenceTraversal {
  typedef std::vector<WordID> Result;
  void operator()(const HG::Edge& edge,
                  const std::vector<const Result*>& ants,
                  Result* result) const {
    edge.rule_->FSubstitute(ants, result);
  }
};

// create a strings of the form (S (X the man) (X said (X he (X would (X go)))))
struct ETreeTraversal {
  ETreeTraversal() : left("("), space(" "), right(")") {}
  const std::string left;
  const std::string space;
  const std::string right;
  typedef std::vector<WordID> Result;
  void operator()(const HG::Edge& edge,
                  const std::vector<const Result*>& ants,
                  Result* result) const {
    Result tmp;
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
  typedef std::vector<WordID> Result;
  void operator()(const HG::Edge& edge,
                  const std::vector<const Result*>& ants,
                  Result* result) const {
    Result tmp;
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
  typedef std::vector<HG::Edge const*> Result;
  void operator()(const HG::Edge& edge,
                  std::vector<Result const*> const& ants,
                  Result* result) const {
    for (unsigned i = 0; i < ants.size(); ++i)
      for (unsigned j = 0; j < ants[i]->size(); ++j)
        result->push_back((*ants[i])[j]);
    result->push_back(&edge);
  }
};

struct FeatureVectorTraversal {
  typedef SparseVector<double> Result;
  void operator()(HG::Edge const& edge,
                  std::vector<Result const*> const& ants,
                  Result* result) const {
    for (unsigned i = 0; i < ants.size(); ++i)
      *result+=*ants[i];
    *result+=edge.feature_values_;
  }
};


std::string JoshuaVisualizationString(const Hypergraph& hg);
prob_t ViterbiESentence(const Hypergraph& hg, std::vector<WordID>* result);
std::string ViterbiETree(const Hypergraph& hg);
void ViterbiRules(const Hypergraph& hg, std::ostream* s);
prob_t ViterbiFSentence(const Hypergraph& hg, std::vector<WordID>* result);
std::string ViterbiFTree(const Hypergraph& hg);
int ViterbiELength(const Hypergraph& hg);
int ViterbiPathLength(const Hypergraph& hg);

/// if weights supplied, assert viterbi prob = features.dot(*weights) (exception if fatal, cerr warn if not).  return features (sum over all edges in viterbi derivation)
SparseVector<double> ViterbiFeatures(Hypergraph const& hg,WeightVector const* weights=0,bool fatal_dotprod_disagreement=false);

#endif
