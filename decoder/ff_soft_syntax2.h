#ifndef _FF_SOFTSYNTAX2_H_
#define _FF_SOFTSYNTAX2_H_

#include "ff.h"
#include "hg.h"

struct SoftSyntacticFeatures2Impl;

class SoftSyntacticFeatures2 : public FeatureFunction {
 public:
  SoftSyntacticFeatures2(const std::string& param);
  ~SoftSyntacticFeatures2();
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  SoftSyntacticFeatures2Impl* impl;
};



#endif
