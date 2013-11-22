#ifndef _DTRAIN_SCORE_H_
#define _DTRAIN_SCORE_H_

#include "dtrain.h"

namespace dtrain
{


struct NgramCounts
{
  unsigned N_;
  map<unsigned, score_t> clipped_;
  map<unsigned, score_t> sum_;

  NgramCounts(const unsigned N) : N_(N) { Zero(); }

  inline void
  operator+=(const NgramCounts& rhs)
  {
    if (rhs.N_ > N_) Resize(rhs.N_);
    for (unsigned i = 0; i < N_; i++) {
      this->clipped_[i] += rhs.clipped_.find(i)->second;
      this->sum_[i] += rhs.sum_.find(i)->second;
    }
  }

  inline const NgramCounts
  operator+(const NgramCounts &other) const
  {
    NgramCounts result = *this;
    result += other;
    return result;
  }

  inline void
  operator*=(const score_t rhs)
  {
    for (unsigned i = 0; i < N_; i++) {
      this->clipped_[i] *= rhs;
      this->sum_[i] *= rhs;
    }
  }

  inline void
  Add(const unsigned count, const unsigned ref_count, const unsigned i)
  {
    assert(i < N_);
    if (count > ref_count) {
      clipped_[i] += ref_count;
    } else {
      clipped_[i] += count;
    }
    sum_[i] += count;
  }

  inline void
  Zero()
  {
    for (unsigned i = 0; i < N_; i++) {
      clipped_[i] = 0.;
      sum_[i] = 0.;
    }
  }

  inline void
  One()
  {
    for (unsigned i = 0; i < N_; i++) {
      clipped_[i] = 1.;
      sum_[i] = 1.;
    }
  }

  inline void
  Print()
  {
    for (unsigned i = 0; i < N_; i++) {
      cout << i+1 << "grams (clipped):\t" << clipped_[i] << endl;
      cout << i+1 << "grams:\t\t\t" << sum_[i] << endl;
    }
  }

  inline void Resize(unsigned N)
  {
    if (N == N_) return;
    else if (N > N_) {
      for (unsigned i = N_; i < N; i++) {
        clipped_[i] = 0.;
        sum_[i] = 0.;
      }
    } else { // N < N_
      for (unsigned i = N_-1; i > N-1; i--) {
        clipped_.erase(i);
        sum_.erase(i);
      }
    }
    N_ = N;
  }
};

typedef map<vector<WordID>, unsigned> Ngrams;

inline Ngrams
make_ngrams(const vector<WordID>& s, const unsigned N)
{
  Ngrams ngrams;
  vector<WordID> ng;
  for (size_t i = 0; i < s.size(); i++) {
    ng.clear();
    for (unsigned j = i; j < min(i+N, s.size()); j++) {
      ng.push_back(s[j]);
      ngrams[ng]++;
    }
  }
  return ngrams;
}

inline NgramCounts
make_ngram_counts(const vector<WordID>& hyp, const vector<WordID>& ref, const unsigned N)
{
  Ngrams hyp_ngrams = make_ngrams(hyp, N);
  Ngrams ref_ngrams = make_ngrams(ref, N);
  NgramCounts counts(N);
  Ngrams::iterator it;
  Ngrams::iterator ti;
  for (it = hyp_ngrams.begin(); it != hyp_ngrams.end(); it++) {
    ti = ref_ngrams.find(it->first);
    if (ti != ref_ngrams.end()) {
      counts.Add(it->second, ti->second, it->first.size() - 1);
    } else {
      counts.Add(it->second, 0, it->first.size() - 1);
    }
  }
  return counts;
}

struct BleuScorer : public LocalScorer
{
  score_t Bleu(NgramCounts& counts, const unsigned hyp_len, const unsigned ref_len);
  score_t Score(const vector<WordID>& hyp, const vector<WordID>& ref, const unsigned /*rank*/, const unsigned /*src_len*/);
  void Reset() {}
};

struct StupidBleuScorer : public LocalScorer
{
  score_t Score(const vector<WordID>& hyp, const vector<WordID>& ref, const unsigned /*rank*/, const unsigned /*src_len*/);
  void Reset() {}
};

struct FixedStupidBleuScorer : public LocalScorer
{
  score_t Score(const vector<WordID>& hyp, const vector<WordID>& ref, const unsigned /*rank*/, const unsigned /*src_len*/);
  void Reset() {}
};

struct SmoothBleuScorer : public LocalScorer
{
  score_t Score(const vector<WordID>& hyp, const vector<WordID>& ref, const unsigned /*rank*/, const unsigned /*src_len*/);
  void Reset() {}
};

struct SumBleuScorer : public LocalScorer
{
  score_t Score(const vector<WordID>& hyp, const vector<WordID>& ref, const unsigned /*rank*/, const unsigned /*src_len*/);
  void Reset() {}
};

struct SumExpBleuScorer : public LocalScorer
{
  score_t Score(const vector<WordID>& hyp, const vector<WordID>& ref, const unsigned /*rank*/, const unsigned /*src_len*/);
  void Reset() {}
};

struct SumWhateverBleuScorer : public LocalScorer
{
  score_t Score(const vector<WordID>& hyp, const vector<WordID>& ref, const unsigned /*rank*/, const unsigned /*src_len*/);
  void Reset() {};
};

struct ApproxBleuScorer : public BleuScorer
{
  NgramCounts glob_onebest_counts_;
  unsigned glob_hyp_len_, glob_ref_len_, glob_src_len_;
  score_t discount_;

  ApproxBleuScorer(unsigned N, score_t d) : glob_onebest_counts_(NgramCounts(N)), discount_(d)
  {
    glob_hyp_len_ = glob_ref_len_ = glob_src_len_ = 0;
  }

  inline void Reset() {
    glob_onebest_counts_.Zero();
    glob_hyp_len_ = glob_ref_len_ = glob_src_len_ = 0.;
  }

  score_t Score(const vector<WordID>& hyp, const vector<WordID>& ref, const unsigned rank, const unsigned src_len);
};

struct LinearBleuScorer : public BleuScorer
{
  unsigned onebest_len_;
  NgramCounts onebest_counts_;

  LinearBleuScorer(unsigned N) : onebest_len_(1), onebest_counts_(N)
  {
    onebest_counts_.One();
  }

  score_t Score(const vector<WordID>& hyp, const vector<WordID>& ref, const unsigned rank, const unsigned /*src_len*/);

  inline void Reset() {
    onebest_len_ = 1;
    onebest_counts_.One();
  }
};


} // namespace

#endif

