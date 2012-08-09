#ifndef _GAMMA_POISSON_H_
#define _GAMMA_POISSON_H_

#include <m.h>

// http://en.wikipedia.org/wiki/Conjugate_prior
struct GammaPoisson {
  GammaPoisson(double shape, double rate) :
    a(shape), b(rate), n(), marginal() {}

  double prob(unsigned x) const {
    return exp(Md::log_negative_binom(x, a + marginal, 1.0 - (b + n) / (1 + b + n)));
  }

  void increment(unsigned x) {
    ++n;
    marginal += x;
  }

  void decrement(unsigned x) {
    --n;
    marginal -= x;
  }

  double log_likelihood() const {
    return 0;
  }

  double a, b;
  int n, marginal;
};

#endif
