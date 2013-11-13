#ifndef _LM_FF_H_
#define _LM_FF_H_

#include <vector>
#include <string>

#include "hg.h"
#include "ff.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// everything in this file is deprecated and may be broken.
// Chris Dyer, Mar 2011

class LanguageModelInterface {
 public:
  double floor_;
  LanguageModelInterface() : floor_(-100) {  }
  virtual ~LanguageModelInterface() {  }

  // not clamped to floor.  log10prob
  virtual double WordProb(WordID word, WordID const* context) = 0;
  inline double WordProbFloored(WordID word, WordID const* context) {
    return clamp(WordProb(word,context));
  }
  // may be shorter than actual null-terminated length.  context must be null terminated.  len is just to save effort for subclasses that don't support contextID
  virtual int ContextSize(WordID const* context,int len) = 0;
  // use this as additional logprob when shortening the context as above
  virtual double ContextBOW(WordID const* context,int shortened_len) = 0; // unlikely that you'll ever need to floor a backoff cost.  i'd say impossible.

  inline double ShortenContext(WordID * context,int len) {
    int slen=ContextSize(context,len);
    double p=ContextBOW(context,slen);
    while (len>slen) {
      --len;
      context[len]=0;
    }
    return p;
  }
  /// should be worse prob = more negative.  that's what SRI wordProb returns: log10(prob)
  inline double clamp(double logp) const {
    return logp < floor_ ? floor_ : logp;
  }
};

struct LanguageModelImpl;

class LanguageModel : public FeatureFunction {
 public:
  // param = "filename.lm [-o n]"
  LanguageModel(const std::string& param);
  ~LanguageModel();
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
  int fid_; // conceptually const; mutable only to simplify constructor
  //LanguageModelImpl &imp() { return *(LanguageModelImpl*)pimpl_; }
  LanguageModelImpl & imp() const { return *(LanguageModelImpl*)pimpl_; }
  /* mutable */ LanguageModelInterface* pimpl_;
};

#endif
