#ifndef _FF_RULES_H_
#define _FF_RULES_H_

#include <vector>
#include <map>
#include "ff.h"
#include "array2d.h"
#include "wordid.h"

class RuleIdentityFeatures : public FeatureFunction {
 public:
  RuleIdentityFeatures(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  mutable std::map<const TRule*, int> rule2_fid_;
};

class RuleNgramFeatures : public FeatureFunction {
 public:
  RuleNgramFeatures(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  mutable std::map<const TRule*, SparseVector<double> > rule2_feats_;
};

#endif
