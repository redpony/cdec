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

struct SoftKBestSynFeatureImpl;

class SoftKBestSynFeature : public FeatureFunction {
 public:
  SoftKBestSynFeature(std::string param);
  ~SoftKBestSynFeature();
  static std::string usage(bool param, bool verbose);

 protected:
  virtual void PrepareForInput(const SentenceMetadata& smeta);

  virtual void TraversalFeaturesImpl(
      const SentenceMetadata& smeta, const HG::Edge& edge,
      const std::vector<const void*>& ant_contexts,
      SparseVector<double>* features, SparseVector<double>* estimated_features,
      void* out_context) const;

 private:
  SoftKBestSynFeatureImpl* pimpl_;
};

struct SoftKBestSynFeatureFactory : public FactoryBase<FeatureFunction> {
  FP Create(std::string param) const;
  std::string usage(bool params, bool verbose) const;
};

struct SoftForestSynFeatureImpl;

class SoftForestSynFeature : public FeatureFunction {
 public:
  SoftForestSynFeature(std::string param);
  ~SoftForestSynFeature();
  static std::string usage(bool param, bool verbose);

 protected:
  virtual void PrepareForInput(const SentenceMetadata& smeta);

  virtual void TraversalFeaturesImpl(
      const SentenceMetadata& smeta, const HG::Edge& edge,
      const std::vector<const void*>& ant_contexts,
      SparseVector<double>* features, SparseVector<double>* estimated_features,
      void* out_context) const;

 private:
  SoftForestSynFeatureImpl* pimpl_;
};

struct SoftForestSynFeatureFactory : public FactoryBase<FeatureFunction> {
  FP Create(std::string param) const;
  std::string usage(bool params, bool verbose) const;
};

#endif /* FF_SOFT_SYN_H_ */
