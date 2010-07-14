#ifndef _COMB_SCORER_
#define _COMB_SCORER_

#include "scorer.h"

class BLEUTERCombinationScorer : public SentenceScorer {
 public:
  BLEUTERCombinationScorer(const std::vector<std::vector<WordID> >& refs);
  ~BLEUTERCombinationScorer();
  Score* ScoreCandidate(const std::vector<WordID>& hyp) const;
  Score* ScoreCCandidate(const std::vector<WordID>& hyp) const;
  static Score* ScoreFromString(const std::string& in);
 private:
  SentenceScorer* bleu_;
  SentenceScorer* ter_;
};

#endif
