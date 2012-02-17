#ifndef _M_H_
#define _M_H_

#include <cassert>
#include <cmath>
#include <boost/math/special_functions/digamma.hpp>

template <typename F>
struct M {
  // support [0, 1, 2 ...)
  static inline F log_poisson(unsigned x, const F& lambda) {
    assert(lambda > 0.0);
    return std::log(lambda) * x - lgamma(x + 1) - lambda;
  }

  // support [0, 1, 2 ...)
  static inline F log_geometric(unsigned x, const F& p) {
    assert(p > 0.0);
    assert(p < 1.0);
    return std::log(1 - p) * x + std::log(p);
  }

  // log of the binomial coefficient
  static inline F log_binom_coeff(unsigned n, unsigned k) {
    assert(n >= k);
    if (n == k) return 0.0;
    return lgamma(n + 1) - lgamma(k + 1) - lgamma(n - k + 1);
  }

  // http://en.wikipedia.org/wiki/Negative_binomial_distribution
  // support [0, 1, 2 ...)
  static inline F log_negative_binom(unsigned x, unsigned r, const F& p) {
    assert(p > 0.0);
    assert(p < 1.0);
    return log_binom_coeff(x + r - 1u, x) + r * std::log(F(1) - p) + x * std::log(p);
  }

  // this is the Beta function, *not* the beta probability density
  // http://mathworld.wolfram.com/BetaFunction.html
  static inline F log_beta_fn(const F& x, const F& y) {
    return lgamma(x) + lgamma(y) - lgamma(x + y);
  }

  // support x >= 0.0
  static F log_gamma_density(const F& x, const F& shape, const F& rate) {
    assert(x >= 0.0);
    assert(shape > 0.0);
    assert(rate > 0.0);
    return (shape-1)*std::log(x) - shape*std::log(rate) - x/rate - lgamma(shape);
  }

  // this is the Beta *density* p(x ; alpha, beta)
  // support x \in (0,1)
  static inline F log_beta_density(const F& x, const F& alpha, const F& beta) {
    assert(x > 0.0);
    assert(x < 1.0);
    assert(alpha > 0.0);
    assert(beta > 0.0);
    return (alpha-1)*std::log(x)+(beta-1)*std::log(1-x) - log_beta_fn(alpha, beta);
  }

  // note: this has been adapted so that 0 is in the support of the distribution
  // support [0, 1, 2 ...)
  static inline F log_yule_simon(unsigned x, const F& rho) {
    assert(rho > 0.0);
    return std::log(rho) + log_beta_fn(x + 1, rho + 1);
  }

  // see http://www.gatsby.ucl.ac.uk/~ywteh/research/compling/hpylm.pdf
  // when y=1, sometimes written x^{\overline{n}} or x^{(n)} "Pochhammer symbol"
  static inline F log_generalized_factorial(const F& x, const F& n, const F& y = 1.0) {
    assert(x > 0.0);
    assert(y >= 0.0);
    assert(n > 0.0);
    if (!n) return 0.0;
    if (y == F(1)) {
      return lgamma(x + n) - lgamma(x);
    } else if (y) {
      return n * std::log(y) + lgamma(x/y + n) - lgamma(x/y);
    } else {  // y == 0.0
      return n * std::log(x);
    }
  }

  // digamma is the first derivative of the log-gamma function
  static inline F digamma(const F& x) {
    return boost::math::digamma(x);
  }

};

typedef M<double> Md;
typedef M<double> Mf;

#endif
