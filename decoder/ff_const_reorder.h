/*
 * ff_const_reorder.h
 *
 *  Created on: Jul 11, 2013
 *      Author: junhuili
 */

#ifndef FF_CONST_REORDER_H_
#define FF_CONST_REORDER_H_

#include "ff_factory.h"
#include "ff.h"

struct ConstReorderFeatureImpl;

class ConstReorderFeature : public FeatureFunction {
 public:
  // param = "filename n"
	ConstReorderFeature(const std::string& param);
    ~ConstReorderFeature();
    static std::string usage(bool param,bool verbose);
 protected:
  virtual void PrepareForInput(const SentenceMetadata& smeta);

  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  ConstReorderFeatureImpl* pimpl_;
};


struct ConstReorderFeatureFactory : public FactoryBase<FeatureFunction> {
  FP Create(std::string param) const;
  std::string usage(bool params,bool verbose) const;
};

#endif /* FF_CONST_REORDER_H_ */
