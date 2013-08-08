#ifndef _FF_SOFTSYNTAX_H_
#define _FF_SOFTSYNTAX_H_

#include "ff.h"
#include "hg.h"

struct SoftSyntacticFeaturesImpl;

class SoftSyntacticFeatures : public FeatureFunction {
 public:
  SoftSyntacticFeatures(const std::string& param);
  ~SoftSyntacticFeatures();
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  SoftSyntacticFeaturesImpl* impl;
};



#endif
