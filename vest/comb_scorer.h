#ifndef _COMB_SCORER_
#define _COMB_SCORER_

#include "scorer.h"

class BLEUTERCombinationScorer : public SentenceScorer {
 public:
  BLEUTERCombinationScorer(const std::vector<std::vector<WordID> >& refs);
  ~BLEUTERCombinationScorer();
  ScoreP ScoreCandidate(const std::vector<WordID>& hyp) const;
  ScoreP ScoreCCandidate(const std::vector<WordID>& hyp) const;
  static ScoreP ScoreFromString(const std::string& in);
 private:
  ScorerP bleu_,ter_;
};

#endif
