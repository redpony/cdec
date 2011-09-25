#ifndef _DTRAIN_SCORE_H_
#define _DTRAIN_SCORE_H_

#include <iostream>
#include <vector>
#include <map>
#include <cassert>
#include <cmath>

#include "wordid.h" // cdec

using namespace std;

namespace dtrain
{


typedef double score_t; // float

struct NgramCounts
{
  size_t N_;
  map<size_t, size_t> clipped;
  map<size_t, size_t> sum;

  NgramCounts(const size_t N) : N_(N) { reset(); } 

  void
  operator+=(const NgramCounts& rhs)
  {
    assert(N_ == rhs.N_);
    for (size_t i = 0; i < N_; i++) {
      this->clipped[i] += rhs.clipped.find(i)->second;
      this->sum[i] += rhs.sum.find(i)->second;
    }
  }

  const NgramCounts
  operator+(const NgramCounts &other) const
  {
    NgramCounts result = *this;
    result += other;
    return result;
  }

  void
  add(size_t count, size_t ref_count, size_t i)
  {
    assert(i < N_);
    if (count > ref_count) {
      clipped[i] += ref_count;
      sum[i] += count;
    } else {
      clipped[i] += count;
      sum[i] += count;
    }
  }

  void
  reset()
  {
    size_t i;
    for (i = 0; i < N_; i++) {
      clipped[i] = 0;
      sum[i] = 0;
    }
  }

  void
  print()
  {
    for (size_t i = 0; i < N_; i++) {
      cout << i+1 << "grams (clipped):\t" << clipped[i] << endl;
      cout << i+1 << "grams:\t\t\t" << sum[i] << endl;
    }
  }
};

typedef map<vector<WordID>, size_t> Ngrams;

Ngrams make_ngrams(vector<WordID>& s, size_t N);
NgramCounts make_ngram_counts(vector<WordID> hyp, vector<WordID> ref, size_t N);

score_t brevity_penaly(const size_t hyp_len, const size_t ref_len);
score_t bleu(NgramCounts& counts, const size_t hyp_len, const size_t ref_len, const size_t N,
             vector<score_t> weights = vector<score_t>());
score_t stupid_bleu(NgramCounts& counts, const size_t hyp_len, const size_t ref_len, size_t N,
                    vector<score_t> weights = vector<score_t>());
score_t smooth_bleu(NgramCounts& counts, const size_t hyp_len, const size_t ref_len, const size_t N,
                    vector<score_t> weights = vector<score_t>());
score_t approx_bleu(NgramCounts& counts, const size_t hyp_len, const size_t ref_len, const size_t N,
                    vector<score_t> weights = vector<score_t>());


} // namespace

#endif

