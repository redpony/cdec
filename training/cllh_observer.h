#ifndef _CLLH_OBSERVER_H_
#define _CLLH_OBSERVER_H_

#include "decoder.h"

struct ConditionalLikelihoodObserver : public DecoderObserver {

  ConditionalLikelihoodObserver() : trg_words(), acc_obj(), cur_obj() {}
  ~ConditionalLikelihoodObserver();

  void Reset() {
    acc_obj = 0;
    trg_words = 0;
  }
 
  virtual void NotifyDecodingStart(const SentenceMetadata&);
  virtual void NotifyTranslationForest(const SentenceMetadata&, Hypergraph* hg);
  virtual void NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg);

  unsigned trg_words;
  double acc_obj;
  double cur_obj;
  int state;
};

#endif
