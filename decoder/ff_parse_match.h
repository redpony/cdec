#ifndef _FF_PARSE_MATCH_H_
#define _FF_PARSE_MATCH_H_

#include "ff.h"
#include "hg.h"

struct ParseMatchFeaturesImpl;

class ParseMatchFeatures : public FeatureFunction {
 public:
  ParseMatchFeatures(const std::string& param);
  ~ParseMatchFeatures();
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  ParseMatchFeaturesImpl* impl;
};

#endif

