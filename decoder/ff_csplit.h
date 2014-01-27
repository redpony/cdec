#ifndef _FF_CSPLIT_H_
#define _FF_CSPLIT_H_

#include <boost/shared_ptr.hpp>

#include "ff.h"
#include "klm/lm/model.hh"

class BasicCSplitFeaturesImpl;
class BasicCSplitFeatures : public FeatureFunction {
 public:
  BasicCSplitFeatures(const std::string& param);
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  boost::shared_ptr<BasicCSplitFeaturesImpl> pimpl_;
};

template <class M> class ReverseCharLMCSplitFeatureImpl;
class ReverseCharLMCSplitFeature : public FeatureFunction {
 public:
  ReverseCharLMCSplitFeature(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  boost::shared_ptr<ReverseCharLMCSplitFeatureImpl<lm::ngram::ProbingModel> > pimpl_;
  const int fid_;
};

#endif
