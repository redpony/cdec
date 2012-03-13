#ifndef _NGRAM_BASE_H_
#define _NGRAM_BASE_H_

#include <string>
#include <vector>
#include "trule.h"
#include "wordid.h"
#include "prob.h"

struct FixedNgramBaseImpl;
struct FixedNgramBase {
  FixedNgramBase(const std::string& lmfname);
  ~FixedNgramBase();
  prob_t StringProbability(const std::vector<WordID>& s) const;

  prob_t operator()(const TRule& rule) const {
    return StringProbability(rule.e_);
  }

 private:
  FixedNgramBaseImpl* impl;

};

#endif
