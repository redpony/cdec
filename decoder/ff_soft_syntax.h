#ifndef _FF_SOFT_SYNTAX_H_
#define _FF_SOFT_SYNTAX_H_

#include "ff.h"
#include "hg.h"

struct SoftSyntaxFeaturesImpl;

class SoftSyntaxFeatures : public FeatureFunction {
 public:
  SoftSyntaxFeatures(const std::string& param);
  ~SoftSyntaxFeatures();
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  SoftSyntaxFeaturesImpl* impl;
};


#endif

