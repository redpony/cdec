#ifndef _TRANSLITERATIONS_H_
#define _TRANSLITERATIONS_H_

#include <vector>
#include "wordid.h"
#include "prob.h"

struct BackwardEstimator;
struct TransliterationsImpl;
struct Transliterations {
  // max_src and max_trg indicate how big the transliteration phrases can be
  // see reachability.h for information about filter_ratio
  explicit Transliterations(int max_src, int max_trg, double s2t_rat, const BackwardEstimator& be);
  ~Transliterations();
  void Initialize(WordID src, const std::vector<WordID>& src_lets, WordID trg, const std::vector<WordID>& trg_lets);
  void Forbid(WordID src, const std::vector<WordID>& src_lets, WordID trg, const std::vector<WordID>& trg_lets);
  void GraphSummary() const;
  prob_t EstimateProbability(WordID s, const std::vector<WordID>& src, WordID t, const std::vector<WordID>& trg) const;
 private:
  TransliterationsImpl* pimpl_;
};

#endif

