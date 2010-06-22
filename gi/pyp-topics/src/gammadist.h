/* gammadist.h -- computes probability of samples under / produces samples from a Gamma distribution
 *
 * Mark Johnson, 22nd March 2008
 *
 * gammavariate() was translated from random.py in Python library
 *
 * The Gamma distribution is:
 *
 *   Gamma(x | alpha, beta) = pow(x/beta, alpha-1) * exp(-x/beta) / (gamma(alpha)*beta)
 *
 * shape parameter alpha > 0 (also called c), scale parameter beta > 0 (also called s); 
 * mean is alpha*beta, variance is alpha*beta**2
 *
 * Note that many parameterizations of the Gamma function are in terms of an _inverse_
 * scale parameter beta, which is the inverse of the beta given here.
 */

#ifndef GAMMADIST_H
#define GAMMADIST_H

#ifdef __cplusplus
extern "C" {
#endif
  
  /* gammadist() returns the probability density of x under a Gamma(alpha,beta) 
   * distribution
   */

  long double gammadist(long double x, long double alpha, long double beta);

  /* lgammadist() returns the log probability density of x under a Gamma(alpha,beta)
   * distribution
   */

  long double lgammadist(long double x, long double alpha, long double beta);

  /* gammavariate() generates samples from a Gamma distribution
   * conditioned on the parameters alpha and beta.
   * 
   * alpha > 0, beta > 0, mean is alpha*beta, variance is alpha*beta**2
   *
   * Warning: a few older sources define the gamma distribution in terms
   * of alpha > -1.0
   */

  long double gammavariate(long double alpha, long double beta);

  /* betadist() returns the probability density of x under a Beta(alpha,beta)
   * distribution.
   */

  long double betadist(long double x, long double alpha, long double beta);

  /* lbetadist() returns the log probability density of x under a Beta(alpha,beta)
   * distribution.
   */
  
  long double lbetadist(long double x, long double alpha, long double beta);

  /* betavariate() generates a sample from a Beta distribution with
   * parameters alpha and beta.
   *
   * 0 < alpha < 1, 0 < beta < 1, mean is alpha/(alpha+beta)
   */

  long double betavariate(long double alpha, long double beta);

#ifdef __cplusplus
};
#endif

#endif /* GAMMADIST_H */
