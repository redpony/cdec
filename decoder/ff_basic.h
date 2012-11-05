#ifndef _FF_BASIC_H_
#define _FF_BASIC_H_

#include "ff.h"

// word penalty feature, for each word on the E side of a rule,
// add value_
class WordPenalty : public FeatureFunction {
 public:
  WordPenalty(const std::string& param);
  static std::string usage(bool p,bool d) {
    return usage_helper("WordPenalty","","number of target words (local feature)",p,d);
  }
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  const int fid_;
  const double value_;
};

class SourceWordPenalty : public FeatureFunction {
 public:
  SourceWordPenalty(const std::string& param);
  static std::string usage(bool p,bool d) {
    return usage_helper("SourceWordPenalty","","number of source words (local feature, and meaningless except when input has non-constant number of source words, e.g. segmentation/morphology/speech recognition lattice)",p,d);
  }
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  const int fid_;
  const double value_;
};

#define DEFAULT_MAX_ARITY 9
#define DEFAULT_MAX_ARITY_STRINGIZE(x) #x
#define DEFAULT_MAX_ARITY_STRINGIZE_EVAL(x) DEFAULT_MAX_ARITY_STRINGIZE(x)
#define DEFAULT_MAX_ARITY_STR DEFAULT_MAX_ARITY_STRINGIZE_EVAL(DEFAULT_MAX_ARITY)

class ArityPenalty : public FeatureFunction {
 public:
  ArityPenalty(const std::string& param);
  static std::string usage(bool p,bool d) {
    return usage_helper("ArityPenalty","[MaxArity(default " DEFAULT_MAX_ARITY_STR ")]","Indicator feature Arity_N=1 for rule of arity N (local feature).  0<=N<=MaxArity(default " DEFAULT_MAX_ARITY_STR ")",p,d);
  }

 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  std::vector<WordID> fids_;
  const double value_;
};

#endif
