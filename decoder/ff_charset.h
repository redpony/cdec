#ifndef _FFCHARSET_H_
#define _FFCHARSET_H_

#include <string>
#include <map>
#include "ff.h"
#include "hg.h"

class SentenceMetadata;

class NonLatinCount : public FeatureFunction {
 public:
  NonLatinCount(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     FeatureVector* features,
                                     FeatureVector* estimated_features,
                                     void* context) const;
 private:
  mutable std::map<WordID, bool> is_non_latin_;
  const int fid_;
};

#endif
