#ifndef _GLOBAL_FF_H_
#define _GLOBAL_FF_H_

#include "arc_factored.h"

struct GFFImpl;
struct GlobalFeatureFunctions {
  GlobalFeatureFunctions();
  ~GlobalFeatureFunctions();
  void PrepareForInput(const TaggedSentence& sentence);
  void Features(const TaggedSentence& sentence,
                const EdgeSubset& tree,
                SparseVector<double>* feats) const;
 private:
  GFFImpl* pimpl;
};

#endif
