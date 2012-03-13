#ifndef _M_H_
#define _M_H_

#include <cassert>
#include <cmath>
#include <boost/math/special_functions/digamma.hpp>
#include <boost/math/constants/constants.hpp>

// TODO right now I sometimes assert that x is in the support of the distributions
// should be configurable to return -inf instead

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

  // support x \in R
  static inline F log_laplace_density(const F& x, const F& mu, const F& b) {
    assert(b > 0.0);
    return -std::log(2*b) - std::fabs(x - mu) / b;
  }

  // support x \in R
  // this is NOT the "log normal" density, it is the log of the "normal density at x"
  static inline F log_gaussian_density(const F& x, const F& mu, const F& var) {
    assert(var > 0.0);
    return -0.5 * std::log(var * 2 * boost::math::constants::pi<F>()) - (x - mu)*(x - mu) / (2 * var);
  }

  // (x1,x2) \in R^2
  // parameterized in terms of two means, a two "variances", a correlation < 1
  static inline F log_bivariate_gaussian_density(const F& x1, const F& x2,
                                                 const F& mu1, const F& mu2,
                                                 const F& var1, const F& var2,
                                                 const F& cor) {
    assert(var1 > 0);
    assert(var2 > 0);
    assert(std::fabs(cor) < 1.0);
    const F cor2 = cor*cor;
    const F var1var22 = var1 * var2;
    const F Z = 0.5 * std::log(var1var22 * (1 - cor2)) + std::log(2 * boost::math::constants::pi<F>());
    return -Z -1.0 / (2 * (1 - cor2)) * ((x1 - mu1)*(x1-mu1) / var1 + (x2-mu2)*(x2-mu2) / var2 - 2*cor*(x1 - mu1)*(x2-mu2) / std::sqrt(var1var22));
  }

  // support x \in [a,b]
  static inline F log_triangle_density(const F& x, const F& a, const F& b, const F& c) {
    assert(a < b);
    assert(a <= c);
    assert(c <= b);
    assert(x >= a);
    assert(x <= b);
    if (x <= c)
      return std::log(2) + std::log(x - a) - std::log(b - a) - std::log(c - a);
    else
      return std::log(2) + std::log(b - x) - std::log(b - a) - std::log(b - c);
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
