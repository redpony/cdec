#ifndef COMB_SCORER_H_
#define COMB_SCORER_H_

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

class BLEUCBLEUCombinationScorer : public SentenceScorer {
 public:
  BLEUCBLEUCombinationScorer(const std::vector<std::vector<WordID> >& refs);
  ~BLEUCBLEUCombinationScorer();
  ScoreP ScoreCandidate(const std::vector<WordID>& hyp) const;
  ScoreP ScoreCCandidate(const std::vector<WordID>& hyp) const;
  static ScoreP ScoreFromString(const std::string& in);
 private:
  ScorerP bleu_, cbleu_;
};

#endif
