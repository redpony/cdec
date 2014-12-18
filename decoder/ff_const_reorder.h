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

// Soft reordering constraint features from
// http://www.aclweb.org/anthology/P14-1106. To train the classifers,
// use utils/const_reorder_model_trainer for constituency reordering
// constraints and utils/argument_reorder_model_trainer for SRL
// reordering constraints.
//
// Input segments should provide path to parse tree (resp. SRL parse)
// as "parse" (resp. "srl") properties.
class ConstReorderFeature : public FeatureFunction {
 public:
  ConstReorderFeature(const std::string& param);
  ~ConstReorderFeature();
  static std::string usage(bool param, bool verbose);

 protected:
  virtual void PrepareForInput(const SentenceMetadata& smeta);

  virtual void TraversalFeaturesImpl(
      const SentenceMetadata& smeta, const HG::Edge& edge,
      const std::vector<const void*>& ant_contexts,
      SparseVector<double>* features, SparseVector<double>* estimated_features,
      void* out_context) const;

 private:
  ConstReorderFeatureImpl* pimpl_;
};

#endif /* FF_CONST_REORDER_H_ */
