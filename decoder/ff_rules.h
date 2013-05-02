#ifndef _FF_RULES_H_
#define _FF_RULES_H_

#include <vector>
#include <map>
#include "trule.h"
#include "ff.h"
#include "hg.h"
#include "array2d.h"
#include "wordid.h"

class RuleIdentityFeatures : public FeatureFunction {
 public:
  RuleIdentityFeatures(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  mutable std::map<const TRule*, int> rule2_fid_;
};

class RuleSourceBigramFeatures : public FeatureFunction {
 public:
  RuleSourceBigramFeatures(const std::string& param);
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

class RuleTargetBigramFeatures : public FeatureFunction {
 public:
  RuleTargetBigramFeatures(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  std::vector<std::string> inds;
  mutable std::map<const TRule*, SparseVector<double> > rule2_feats_;
};

#endif
