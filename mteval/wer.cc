#include "wer.h"

#include <cstdio>
#include <cassert>
#include <iostream>
#include <limits>
#include <sstream>
#ifndef HAVE_OLD_CPP
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std { using std::tr1::unordered_map; }
#endif
#include <set>
#include <valarray>
#include <boost/functional/hash.hpp>
#include <stdexcept>
#include "tdict.h"
#include "levenshtein.h"

using namespace std;

class WERScore : public ScoreBase<WERScore> {
  friend class WERScorer;

 public:
  static const unsigned kEDITDISTANCE = 0;
  static const unsigned kCHARCOUNT = 1;
  static const unsigned kDUMMY_LAST_ENTRY = 2;

 WERScore() : stats(0,kDUMMY_LAST_ENTRY) {}
  float ComputePartialScore() const { return 0.0;}
  float ComputeScore() const {
    if (static_cast<float>(stats[kCHARCOUNT]) < 0.5)
      return 0;
    return static_cast<float>(stats[kEDITDISTANCE]) / static_cast<float>(stats[kCHARCOUNT]);
  }
  void ScoreDetails(string* details) const;
  void PlusPartialEquals(const Score& rhs, int oracle_e_cover, int oracle_f_cover, int src_len){}
  void PlusEquals(const Score& delta, const float scale) {
    const WERScore& delta_stats = static_cast<const WERScore&>(delta);
    for (unsigned i = 0; i < kDUMMY_LAST_ENTRY; ++i) {
        stats[i] += scale * static_cast<float>(delta_stats.stats[i]);
    }
 }
  void PlusEquals(const Score& delta) {
    stats += static_cast<const WERScore&>(delta).stats;
  }

  ScoreP GetZero() const {
    return ScoreP(new WERScore);
  }
  ScoreP GetOne() const {
    return ScoreP(new WERScore);
  }
  void Subtract(const Score& rhs, Score* res) const {
    static_cast<WERScore*>(res)->stats = stats - static_cast<const WERScore&>(rhs).stats;
  }
  void Encode(std::string* out) const {
    ostringstream os;
    os << stats[kEDITDISTANCE] << ' '
       << stats[kCHARCOUNT];
    *out = os.str();
  }
  bool IsAdditiveIdentity() const {
    for (int i = 0; i < kDUMMY_LAST_ENTRY; ++i)
      if (stats[i] != 0) return false;
    return true;
  }
 private:
  valarray<int> stats;
};

ScoreP WERScorer::ScoreFromString(const std::string& data) {
  istringstream is(data);
  WERScore* r = new WERScore;
  is >> r->stats[WERScore::kEDITDISTANCE]
     >> r->stats[WERScore::kCHARCOUNT];
  return ScoreP(r);
}

void WERScore::ScoreDetails(std::string* details) const {
  char buf[200];
  sprintf(buf, "WER = %.2f, edits=%d, len=%d",
     ComputeScore() * 100.0f,
     stats[kEDITDISTANCE],
     stats[kCHARCOUNT]);
  *details = buf;
}

WERScorer::~WERScorer() {}
WERScorer::WERScorer(const vector<vector<WordID> >& refs) {this->refs = refs;}

ScoreP WERScorer::ScoreCCandidate(const vector<WordID>& hyp) const {
  return ScoreP();
}

float WERScorer::Calculate(const std::vector<WordID>& hyp, const Sentence& ref, int& edits, int& char_count) const {
  edits = cdec::LevenshteinDistance(hyp, ref);
  char_count = ref.size();
  if (char_count == 0) {
    return 0;
  }
  return static_cast<float>(edits) / static_cast<float>(char_count);
}

ScoreP WERScorer::ScoreCandidate(const std::vector<WordID>& hyp) const {
  float best_score = numeric_limits<float>::max();
  WERScore* res = new WERScore;
  for (int i = 0; i < refs.size(); ++i) {
    int edits, char_count;
    const vector<WordID>& ref = refs[i];
    float score = Calculate(hyp, ref, edits, char_count);
    if (score < best_score) {
      res->stats[WERScore::kEDITDISTANCE] = edits;
      res->stats[WERScore::kCHARCOUNT] = char_count;
      best_score = score;
    }
  }
  return ScoreP(res);
}
