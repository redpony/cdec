#ifndef _LM_FF_H_
#define _LM_FF_H_

#include <vector>
#include <string>

#include "hg.h"
#include "ff.h"
#include "config.h"

class LanguageModelImpl;

class LanguageModel : public FeatureFunction {
 public:
  // param = "filename.lm [-o n]"
  LanguageModel(const std::string& param);
  ~LanguageModel();
  virtual void FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const;
  std::string DebugStateToString(const void* state) const;
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  const int fid_;
  mutable LanguageModelImpl* pimpl_;
};

#ifdef HAVE_RANDLM
class LanguageModelRandLM : public FeatureFunction {
 public:
  // param = "filename.lm [-o n]"
  LanguageModelRandLM(const std::string& param);
  ~LanguageModelRandLM();
  virtual void FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const;
  std::string DebugStateToString(const void* state) const;
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  const int fid_;
  mutable LanguageModelImpl* pimpl_;
};
#endif

#endif
