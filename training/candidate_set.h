#ifndef _CANDIDATE_SET_H_
#define _CANDIDATE_SET_H_

#include <vector>
#include <algorithm>

#include "ns.h"
#include "wordid.h"
#include "sparse_vector.h"

class Hypergraph;

namespace training {

struct Candidate {
  Candidate() {}
  Candidate(const std::vector<WordID>& e, const SparseVector<double>& fm) :
    ewords(e),
    fmap(fm) {}
  std::vector<WordID> ewords;
  SparseVector<double> fmap;
  SufficientStats score_stats;
  void swap(Candidate& other) {
    score_stats.swap(other.score_stats);
    ewords.swap(other.ewords);
    fmap.swap(other.fmap);
  }
};

// represents some kind of collection of translation candidates, e.g.
// aggregated k-best lists, sample lists, etc.
class CandidateSet {
 public:
  CandidateSet() {}
  inline size_t size() const { return cs.size(); }
  const Candidate& operator[](size_t i) const { return cs[i]; }

  void ReadFromFile(const std::string& file);
  void WriteToFile(const std::string& file) const;
  void AddKBestCandidates(const Hypergraph& hg, size_t kbest_size, const SegmentEvaluator* scorer = NULL);
  // TODO add code to do unique k-best
  // TODO add code to draw k samples

 private:
  void Dedup();
  std::vector<Candidate> cs;
};

}

#endif
