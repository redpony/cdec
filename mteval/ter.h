#ifndef _TER_H_
#define _TER_H_

#include "scorer.h"

class TERScorerImpl;

class TERScorer : public SentenceScorer {
 public:
  TERScorer(const std::vector<std::vector<WordID> >& references);
  ~TERScorer();
  ScoreP ScoreCandidate(const std::vector<WordID>& hyp) const;
  ScoreP ScoreCCandidate(const std::vector<WordID>& hyp) const;
  static ScoreP ScoreFromString(const std::string& data);
 private:
  std::vector<TERScorerImpl*> impl_;
};

#endif
