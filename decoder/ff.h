#ifndef _FF_H_
#define _FF_H_

#include <string>
#include <vector>
#include "sparse_vector.h"

namespace HG { struct Edge; struct Node; }
class Hypergraph;
class SentenceMetadata;

// if you want to develop a new feature, inherit from this class and
// override TraversalFeaturesImpl(...).  If it's a feature that returns /
// depends on context, you may also need to implement
// FinalTraversalFeatures(...)
class FeatureFunction {
  friend class ExternalFeature;
 public:
  std::string name_; // set by FF factory using usage()
  FeatureFunction() : state_size_() {}
  explicit FeatureFunction(int state_size) : state_size_(state_size) {}
  virtual ~FeatureFunction();
  bool IsStateful() const { return state_size_ > 0; }
  int StateSize() const { return state_size_; }

  // override this.  not virtual because we want to expose this to factory template for help before creating a FF
  static std::string usage(bool show_params,bool show_details) {
    return usage_helper("FIXME_feature_needs_name","[no parameters]","[no documentation yet]",show_params,show_details);
  }
  static std::string usage_helper(std::string const& name,std::string const& params,std::string const& details,bool show_params,bool show_details);

  // called once, per input, before any feature calls to TraversalFeatures, etc.
  // used to initialize sentence-specific data structures
  virtual void PrepareForInput(const SentenceMetadata& smeta);

  // Compute the feature values and (if this applies) the estimates of the
  // feature values when this edge is used incorporated into a larger context
  inline void TraversalFeatures(const SentenceMetadata& smeta,
                                const HG::Edge& edge,
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
  // it here.  For example, a language model might the cost of adding
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
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;

  // !!! ONLY call this from subclass *CONSTRUCTORS* !!!
  void SetStateSize(size_t state_size) {
    state_size_ = state_size;
  }
 private:
  int state_size_;
};

#endif
