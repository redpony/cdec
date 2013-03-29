#ifndef _FF_SOURCE_PATH_H_
#define _FF_SOURCE_PATH_H_

#include <vector>
#include <map>
#include "ff.h"

class SourcePathFeatures : public FeatureFunction {
 public:
  SourcePathFeatures(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;

 private:
  void FireBigramFeature(WordID prev, WordID cur, SparseVector<double>* features) const;
  void FireUnigramFeature(WordID cur, SparseVector<double>* features) const;
  mutable std::map<WordID, std::map<WordID, int> > bigram_fids;
  mutable std::map<WordID, int> unigram_fids;
};

#endif
