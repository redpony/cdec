#ifndef _FF_SOURCE_TOOLS2_H_
#define _FF_SOURCE_TOOLS2_H_

#include "ff.h"
#include "hg.h"

struct SourceSyntaxFeatures2Impl;

class SourceSyntaxFeatures2 : public FeatureFunction {
 public:
  SourceSyntaxFeatures2(const std::string& param);
  ~SourceSyntaxFeatures2();
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  SourceSyntaxFeatures2Impl* impl;
};

#endif
