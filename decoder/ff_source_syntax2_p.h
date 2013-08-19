#ifndef _FF_SOURCE_TOOLS2_H_
#define _FF_SOURCE_TOOLS2_H_

#include "ff.h"
#include "hg.h"

struct PSourceSyntaxFeatures2Impl;

class PSourceSyntaxFeatures2 : public FeatureFunction {
 public:
  PSourceSyntaxFeatures2(const std::string& param);
  ~PSourceSyntaxFeatures2();
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  PSourceSyntaxFeatures2Impl* impl;
};

#endif
