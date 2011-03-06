#ifndef _KLM_FF_H_
#define _KLM_FF_H_

#include <vector>
#include <string>

#include "ff.h"
#include "lm/model.hh"

template <class Model> struct KLanguageModelImpl;

// the supported template types are instantiated explicitly
// in ff_klm.cc.
template <class Model>
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
  int oov_fid_; // will be zero if extra OOV feature is not configured by decoder
  KLanguageModelImpl<Model>* pimpl_;
};

#endif
