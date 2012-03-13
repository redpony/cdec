#ifndef _FF_CONTEXT_H_
#define _FF_CONTEXT_H_

#include <vector>
#include "ff.h"

class RuleContextFeatures : public FeatureFunction {
 public:
  RuleContextFeatures(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  std::vector<WordID> current_input;
  WordID kSOS, kEOS;
};

#endif
