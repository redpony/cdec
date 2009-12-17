#ifndef _FF_TAGGER_H_
#define _FF_TAGGER_H_

#include <map>
#include "ff.h"

typedef std::map<WordID, int> Class2FID;
typedef std::map<WordID, Class2FID> Class2Class2FID;

// the reason this is a "tagger" feature is that it assumes that
// the sequence unfolds from left to right, which means it doesn't
// have to split states based on left context.
// fires unigram features as well
class Tagger_BigramIdentity : public FeatureFunction {
 public:
  Tagger_BigramIdentity(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  void FireFeature(const WordID& left,
                   const WordID& right,
                   SparseVector<double>* features) const;
  mutable Class2Class2FID fmap_;
};

// for each pair of symbols cooccuring in a lexicalized rule, fire
// a feature (mostly used for tagging, but could be used for any model)
class LexicalPairIdentity : public FeatureFunction {
 public:
  LexicalPairIdentity(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  void FireFeature(WordID src,
                   WordID trg,
                   SparseVector<double>* features) const;
  mutable Class2Class2FID fmap_;
};


#endif
