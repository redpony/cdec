#ifndef _TRANSLITERATIONS_H_
#define _TRANSLITERATIONS_H_

#include <vector>
#include "wordid.h"
#include "prob.h"

struct TransliterationsImpl;
struct Transliterations {
  explicit Transliterations();
  ~Transliterations();
  void Initialize(WordID src, const std::vector<WordID>& src_lets, WordID trg, const std::vector<WordID>& trg_lets);
  void Forbid(WordID src, WordID trg);
  void GraphSummary() const;
  prob_t EstimateProbability(WordID src, WordID trg) const;
  TransliterationsImpl* pimpl_;
};

#endif

