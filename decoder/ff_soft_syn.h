/*
 * ff_soft_syn.h
 *
 */

#ifndef FF_SOFT_SYN_H_
#define FF_SOFT_SYN_H_

#include "ff_factory.h"
#include "ff.h"

struct SoftSynFeatureImpl;

class SoftSynFeature : public FeatureFunction {
 public:
  SoftSynFeature(std::string param);
  ~SoftSynFeature();
  static std::string usage(bool param, bool verbose);

 protected:
  virtual void PrepareForInput(const SentenceMetadata& smeta);

  virtual void TraversalFeaturesImpl(
      const SentenceMetadata& smeta, const HG::Edge& edge,
      const std::vector<const void*>& ant_contexts,
      SparseVector<double>* features, SparseVector<double>* estimated_features,
      void* out_context) const;

 private:
  SoftSynFeatureImpl* pimpl_;
};

struct SoftSynFeatureFactory : public FactoryBase<FeatureFunction> {
  FP Create(std::string param) const;
  std::string usage(bool params, bool verbose) const;
};

#endif /* FF_SOFT_SYN_H_ */
