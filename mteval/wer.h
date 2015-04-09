#ifndef WER_H_
#define WER_H_

#include "scorer.h"

class WERScorer : public SentenceScorer {
 public:
  WERScorer(const std::vector<std::vector<WordID> >& references);
  ~WERScorer();
  ScoreP ScoreCandidate(const std::vector<WordID>& hyp) const;
  ScoreP ScoreCCandidate(const std::vector<WordID>& hyp) const;
  static ScoreP ScoreFromString(const std::string& data);
  float Calculate(const std::vector<WordID>& hyp, const Sentence& ref, int& edits, int& char_count) const;
};

#endif
