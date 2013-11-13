#ifndef _FF_SOURCE_SYNTAX_H_
#define _FF_SOURCE_SYNTAX_H_

#include "ff.h"
#include "hg.h"

struct SourceSyntaxFeaturesImpl;

class SourceSyntaxFeatures : public FeatureFunction {
 public:
  SourceSyntaxFeatures(const std::string& param);
  ~SourceSyntaxFeatures();
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  SourceSyntaxFeaturesImpl* impl;
};

struct SourceSpanSizeFeaturesImpl;
class SourceSpanSizeFeatures : public FeatureFunction {
 public:
  SourceSpanSizeFeatures(const std::string& param);
  ~SourceSpanSizeFeatures();
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  SourceSpanSizeFeaturesImpl* impl;
};

#endif

