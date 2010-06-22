#ifndef _FF_H_
#define _FF_H_

#include <vector>

#include "fdict.h"
#include "hg.h"

class SentenceMetadata;
class FeatureFunction;  // see definition below

// if you want to develop a new feature, inherit from this class and
// override TraversalFeaturesImpl(...).  If it's a feature that returns /
// depends on context, you may also need to implement
// FinalTraversalFeatures(...)
class FeatureFunction {
 public:
  FeatureFunction() : state_size_() {}
  explicit FeatureFunction(int state_size) : state_size_(state_size) {}
  virtual ~FeatureFunction();

  // returns the number of bytes of context that this feature function will
  // (maximally) use.  By default, 0 ("stateless" models in Hiero/Joshua).
  // NOTE: this value is fixed for the instance of your class, you cannot
  // use different amounts of memory for different nodes in the forest.
  inline int NumBytesContext() const { return state_size_; }

  // Compute the feature values and (if this applies) the estimates of the
  // feature values when this edge is used incorporated into a larger context
  inline void TraversalFeatures(const SentenceMetadata& smeta,
                                const Hypergraph::Edge& edge,
                                const std::vector<const void*>& ant_contexts,
                                SparseVector<double>* features,
                                SparseVector<double>* estimated_features,
                                void* out_state) const {
    TraversalFeaturesImpl(smeta, edge, ant_contexts,
                          features, estimated_features, out_state);
    // TODO it's easy for careless feature function developers to overwrite
    // the end of their state and clobber someone else's memory.  These bugs
    // will be horrendously painful to track down.  There should be some
    // optional strict mode that's enforced here that adds some kind of
    // barrier between the blocks reserved for the residual contexts
  }

  // if there's some state left when you transition to the goal state, score
  // it here.  For example, the language model computes the cost of adding
  // <s> and </s>.
  virtual void FinalTraversalFeatures(const void* residual_state,
                                      SparseVector<double>* final_features) const;

 protected:
  // context is a pointer to a buffer of size NumBytesContext() that the
  // feature function can write its state to.  It's up to the feature function
  // to determine how much space it needs and to determine how to encode its
  // residual contextual information since it is OPAQUE to all clients outside
  // of the particular FeatureFunction class.  There is one exception:
  // equality of the contents (i.e., memcmp) is required to determine whether
  // two states can be combined.
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const = 0;

  // !!! ONLY call this from subclass *CONSTRUCTORS* !!!
  void SetStateSize(size_t state_size) {
    state_size_ = state_size;
  }

 private:
  int state_size_;
};

// word penalty feature, for each word on the E side of a rule,
// add value_
class WordPenalty : public FeatureFunction {
 public:
  WordPenalty(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  const int fid_;
  const double value_;
};

class SourceWordPenalty : public FeatureFunction {
 public:
  SourceWordPenalty(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  const int fid_;
  const double value_;
};

class ArityPenalty : public FeatureFunction {
 public:
  ArityPenalty(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  int fids_[10];
  const double value_;
};

// this class is a set of FeatureFunctions that can be used to score, rescore,
// etc. a (translation?) forest
class ModelSet {
 public:
  ModelSet() : state_size_(0) {}

  ModelSet(const std::vector<double>& weights,
           const std::vector<const FeatureFunction*>& models);

  // sets edge->feature_values_ and edge->edge_prob_
  // NOTE: edge must not necessarily be in hg.edges_ but its TAIL nodes
  // must be.
  void AddFeaturesToEdge(const SentenceMetadata& smeta,
                         const Hypergraph& hg,
                         const std::vector<std::string>& node_states,
                         Hypergraph::Edge* edge,
                         std::string* residual_context,
                         prob_t* combination_cost_estimate = NULL) const;

  void AddFinalFeatures(const std::string& residual_context,
                        Hypergraph::Edge* edge) const;

  bool empty() const { return models_.empty(); }
 private:
  std::vector<const FeatureFunction*> models_;
  std::vector<double> weights_; 
  int state_size_;
  std::vector<int> model_state_pos_;
};

#endif
