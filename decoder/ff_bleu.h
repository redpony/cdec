#ifndef _BLEU_FF_H_
#define _BLEU_FF_H_

#include <vector>
#include <string>

#include "hg.h"
#include "ff.h"

class BLEUModelImpl;

class BLEUModel : public FeatureFunction {
 public:
  // param = "filename.lm [-o n]"
  BLEUModel(const std::string& param);
  ~BLEUModel();
  virtual void FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const;
  std::string DebugStateToString(const void* state) const;
  static std::string usage(bool param,bool verbose);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  const int fid_;
  mutable BLEUModelImpl* pimpl_;
};
#endif
