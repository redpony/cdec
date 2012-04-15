#ifndef _ARC_FF_H_
#define _ARC_FF_H_

#include <string>
#include "sparse_vector.h"
#include "weights.h"
#include "arc_factored.h"

struct TaggedSentence;
class ArcFeatureFunction {
 public:
  virtual ~ArcFeatureFunction();

  // called once, per input, before any calls to EdgeFeatures
  // used to initialize sentence-specific data structures
  virtual void PrepareForInput(const TaggedSentence& sentence);

  inline void EgdeFeatures(const TaggedSentence& sentence,
                           short h,
                           short m,
                           SparseVector<weight_t>* features) const {
    EdgeFeaturesImpl(sentence, h, m, features);
  }
 protected:
  virtual void EdgeFeaturesImpl(const TaggedSentence& sentence,
                                short h,
                                short m,
                                SparseVector<weight_t>* features) const = 0;
};

class DistancePenalty : public ArcFeatureFunction {
 public:
  DistancePenalty(const std::string& param);
 protected:
  virtual void EdgeFeaturesImpl(const TaggedSentence& sentence,
                                short h,
                                short m,
                                SparseVector<weight_t>* features) const;
 private:
  const int fidw_, fidr_;
};

#endif
