#ifndef _FF_SOURCE_TOOLS_H_
#define _FF_SOURCE_TOOLS_H_

#include "ff.h"
#include "hg.h"

struct PSourceSyntaxFeaturesImpl;

class PSourceSyntaxFeatures : public FeatureFunction {
 public:
  PSourceSyntaxFeatures(const std::string& param);
  ~PSourceSyntaxFeatures();
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  PSourceSyntaxFeaturesImpl* impl;
};

struct PSourceSpanSizeFeaturesImpl;
class PSourceSpanSizeFeatures : public FeatureFunction {
 public:
  PSourceSpanSizeFeatures(const std::string& param);
  ~PSourceSpanSizeFeatures();
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  PSourceSpanSizeFeaturesImpl* impl;
};

#endif
