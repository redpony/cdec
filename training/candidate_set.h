#ifndef _CANDIDATE_SET_H_
#define _CANDIDATE_SET_H_

#include <vector>
#include <algorithm>

#include "wordid.h"
#include "sparse_vector.h"

class Hypergraph;
struct SegmentEvaluator;
struct EvaluationMetric;

namespace training {

struct Candidate {
  Candidate() : g_(-100.0f) {}
  Candidate(const std::vector<WordID>& e, const SparseVector<double>& fm) : ewords(e), fmap(fm), g_(-100.0f) {}
  std::vector<WordID> ewords;
  SparseVector<double> fmap;
  double g(const SegmentEvaluator& scorer, const EvaluationMetric* metric) const;
  void swap(Candidate& other) {
    std::swap(g_, other.g_);
    ewords.swap(other.ewords);
    fmap.swap(other.fmap);
  }
 private:
  mutable float g_;
  //SufficientStats score_stats;
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
  void AddKBestCandidates(const Hypergraph& hg, size_t kbest_size);
  // TODO add code to do unique k-best
  // TODO add code to draw k samples

 private:
  void Dedup();
  std::vector<Candidate> cs;
};

}

#endif
