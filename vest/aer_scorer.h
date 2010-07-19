#ifndef _AER_SCORER_
#define _AER_SCORER_

#include <boost/shared_ptr.hpp>

#include "scorer.h"
#include "array2d.h"

class AERScorer : public SentenceScorer {
 public:
  // when constructing alignment strings from a hypergraph, the source
  // is necessary.
  AERScorer(const std::vector<std::vector<WordID> >& refs, const std::string& src = "");
  ScoreP ScoreCandidate(const std::vector<WordID>& hyp) const;
  ScoreP ScoreCCandidate(const std::vector<WordID>& hyp) const;
  static ScoreP ScoreFromString(const std::string& in);
  const std::string* GetSource() const;
 private:
  std::string src_;
  boost::shared_ptr<Array2D<bool> > ref_;
};

#endif
