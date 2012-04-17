#ifndef _ARC_FF_H_
#define _ARC_FF_H_

#include <string>
#include "sparse_vector.h"
#include "weights.h"
#include "arc_factored.h"

struct TaggedSentence;
struct ArcFFImpl;
class ArcFeatureFunctions {
 public:
  ArcFeatureFunctions();
  ~ArcFeatureFunctions();

  // called once, per input, before any calls to EdgeFeatures
  // used to initialize sentence-specific data structures
  void PrepareForInput(const TaggedSentence& sentence);

  void EdgeFeatures(const TaggedSentence& sentence,
                    short h,
                    short m,
                    SparseVector<weight_t>* features) const;
 private:
  ArcFFImpl* pimpl;
};

#endif
