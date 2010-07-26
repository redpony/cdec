#ifndef FF_LM_FSA_H
#define FF_LM_FSA_H

//TODO: use SRI LM::contextID to shorten state
//TODO: expose ScanPhrase interface to achieve > ngram probs (e.g. unigram) with higher order lm - but that wouldn't apply to L->R maximal hook/sharing decoding

#include "ff_lm.h"
#include "ff_from_fsa.h"

struct LanguageModelFsa : public FsaFeatureFunctionBase<LanguageModelFsa> {
  // overrides; implementations in ff_lm.cc
  static std::string usage(bool,bool);
  LanguageModelFsa(std::string const& param);
  int markov_order() const { return ctxlen_; }
  void Scan(SentenceMetadata const& /* smeta */,const Hypergraph::Edge& /* edge */,WordID w,void const* old_st,void *new_st,FeatureVector *features) const;
  void print_state(std::ostream &,void *) const;

  // impl details:
  void set_ngram_order(int i); // if you build ff_from_fsa first, then increase this, you will get memory overflows.  otherwise, it's the same as a "-o i" argument to constructor
  double floor_; // log10prob minimum used (e.g. unk words)
private:
  int ngram_order_;
  int ctxlen_; // 1 less than above
  LanguageModelImpl *pimpl_;
};

typedef FeatureFunctionFromFsa<LanguageModelFsa> LanguageModelFromFsa;

#endif
