#ifndef _FF_SOFT_SYNTAX_MINDIST_H_
#define _FF_SOFT_SYNTAX_MINDIST_H_

#include "ff.h"
#include "hg.h"

struct SoftSyntaxFeaturesMindistImpl;

class SoftSyntaxFeaturesMindist : public FeatureFunction {
 public:
  SoftSyntaxFeaturesMindist(const std::string& param);
  ~SoftSyntaxFeaturesMindist();
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  SoftSyntaxFeaturesMindistImpl* impl;
};


#endif

