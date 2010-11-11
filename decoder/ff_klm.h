#ifndef _KLM_FF_H_
#define _KLM_FF_H_

#include <vector>
#include <string>

#include "ff.h"

struct KLanguageModelImpl;

class KLanguageModel : public FeatureFunction {
 public:
  // param = "filename.lm [-o n]"
  KLanguageModel(const std::string& param);
  ~KLanguageModel();
  virtual void FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const;
  static std::string usage(bool param,bool verbose);
  Features features() const;
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  int fid_; // conceptually const; mutable only to simplify constructor
  KLanguageModelImpl* pimpl_;
};

#endif
