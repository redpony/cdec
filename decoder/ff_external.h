#ifndef _FFEXTERNAL_H_
#define _FFEXTERNAL_H_

#include "ff.h"

// dynamically loaded feature function
class ExternalFeature : public FeatureFunction {
 public:
  ExternalFeature(const std::string& param);
  ~ExternalFeature();
  virtual void PrepareForInput(const SentenceMetadata& smeta);
  virtual void FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const;
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  void* lib_handle;
  FeatureFunction* ff_ext;
};

#endif
