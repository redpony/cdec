#ifndef _FFSET_H_
#define _FFSET_H_

#include <vector>
#include "value_array.h"
#include "prob.h"

namespace HG { struct Edge; struct Node; }
class Hypergraph;
class FeatureFunction;
class SentenceMetadata;
class FeatureFunction;  // see definition below

// TODO let states be dynamically sized
typedef ValueArray<uint8_t> FFState; // this is a fixed array, but about 10% faster than string

//FIXME: only context.data() is required to be contiguous, and it becomes invalid after next string operation.  use ValueArray instead? (higher performance perhaps, save a word due to fixed size)
typedef std::vector<FFState> FFStates;

// this class is a set of FeatureFunctions that can be used to score, rescore,
// etc. a (translation?) forest
class ModelSet {
 public:
  ModelSet(const std::vector<double>& weights,
           const std::vector<const FeatureFunction*>& models);

  // sets edge->feature_values_ and edge->edge_prob_
  // NOTE: edge must not necessarily be in hg.edges_ but its TAIL nodes
  // must be.  edge features are supposed to be overwritten, not added to (possibly because rule features aren't in ModelSet so need to be left alone
  void AddFeaturesToEdge(const SentenceMetadata& smeta,
                         const Hypergraph& hg,
                         const FFStates& node_states,
                         HG::Edge* edge,
                         FFState* residual_context,
                         prob_t* combination_cost_estimate = NULL) const;

  //this is called INSTEAD of above when result of edge is goal (must be a unary rule - i.e. one variable, but typically it's assumed that there are no target terminals either (e.g. for LM))
  void AddFinalFeatures(const FFState& residual_context,
                        HG::Edge* edge,
                        SentenceMetadata const& smeta) const;

  // this is called once before any feature functions apply to a hypergraph
  // it can be used to initialize sentence-specific data structures
  void PrepareForInput(const SentenceMetadata& smeta);

  bool empty() const { return models_.empty(); }

  bool stateless() const { return !state_size_; }

 private:
  std::vector<const FeatureFunction*> models_;
  const std::vector<double>& weights_;
  int state_size_;
  std::vector<int> model_state_pos_;
};

#endif
