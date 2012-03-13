#ifndef _PYP_WORD_MODEL_H_
#define _PYP_WORD_MODEL_H_

#include <iostream>
#include <cmath>
#include <vector>
#include "prob.h"
#include "ccrp.h"
#include "m.h"
#include "tdict.h"
#include "os_phrase.h"

// PYP(d,s,poisson-uniform) represented as a CRP
struct PYPWordModel {
  explicit PYPWordModel(const unsigned vocab_e_size, const double mean_len = 5) :
      base(prob_t::One()), r(1,1,1,1,0.66,50.0), u0(-std::log(vocab_e_size)), mean_length(mean_len) {}

  void ResampleHyperparameters(MT19937* rng);

  inline prob_t operator()(const std::vector<WordID>& s) const {
    return r.prob(s, p0(s));
  }

  inline void Increment(const std::vector<WordID>& s, MT19937* rng) {
    if (r.increment(s, p0(s), rng))
      base *= p0(s);
  }

  inline void Decrement(const std::vector<WordID>& s, MT19937 *rng) {
    if (r.decrement(s, rng))
      base /= p0(s);
  }

  inline prob_t Likelihood() const {
    prob_t p; p.logeq(r.log_crp_prob());
    p *= base;
    return p;
  }

  void Summary() const;

 private:
  inline double logp0(const std::vector<WordID>& s) const {
    return Md::log_poisson(s.size(), mean_length) + s.size() * u0;
  }

  inline prob_t p0(const std::vector<WordID>& s) const {
    prob_t p; p.logeq(logp0(s));
    return p;
  }

  prob_t base;  // keeps track of the draws from the base distribution
  CCRP<std::vector<WordID> > r;
  const double u0;  // uniform log prob of generating a letter
  const double mean_length;  // mean length of a word in the base distribution
};

#endif
