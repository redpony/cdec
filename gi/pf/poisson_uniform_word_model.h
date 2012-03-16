#ifndef _POISSON_UNIFORM_WORD_MODEL_H_
#define _POISSON_UNIFORM_WORD_MODEL_H_

#include <cmath>
#include <vector>
#include "prob.h"
#include "m.h"

// len ~ Poisson(lambda)
//   for (1..len)
//     e_i ~ Uniform({Vocabulary})
struct PoissonUniformWordModel {
  explicit PoissonUniformWordModel(const unsigned vocab_size,
                                   const unsigned alphabet_size,
                                   const double mean_len = 5) :
    lh(prob_t::One()),
    v0(-std::log(vocab_size)),
    u0(-std::log(alphabet_size)),
    mean_length(mean_len) {}

  void ResampleHyperparameters(MT19937*) {}

  inline prob_t operator()(const std::vector<WordID>& s) const {
    prob_t p;
    p.logeq(Md::log_poisson(s.size(), mean_length) + s.size() * u0);
    //p.logeq(v0);
    return p;
  }

  inline void Increment(const std::vector<WordID>& w, MT19937*) {
    lh *= (*this)(w);
  }

  inline void Decrement(const std::vector<WordID>& w, MT19937 *) {
    lh /= (*this)(w);
  }

  inline prob_t Likelihood() const { return lh; }

  void Summary() const {}

 private:

  prob_t lh;  // keeps track of the draws from the base distribution
  const double v0;  // uniform log prob of generating a word
  const double u0;  // uniform log prob of generating a letter
  const double mean_length;  // mean length of a word in the base distribution
};

#endif
