/* gammadist.c -- computes probability of samples under / produces samples from a Gamma distribution
 *
 * Mark Johnson, 22nd March 2008
 *
 * WARNING: you need to set the flag -std=c99 to compile
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
 *
 * To define a main() that tests the routines, uncomment the following #define:
 */
/* #define GAMMATEST */

#include <assert.h>
#include <math.h> 

#include "gammadist.h"
#include "mt19937ar.h"

/* gammadist() returns the probability density of x under a Gamma(alpha,beta) 
 * distribution
 */

long double gammadist(long double x, long double alpha, long double beta) {
  assert(alpha > 0);
  assert(beta > 0);
  return  pow(x/beta, alpha-1) * exp(-x/beta) / (tgamma(alpha)*beta);
}

/* lgammadist() returns the log probability density of x under a Gamma(alpha,beta)
 * distribution
 */

long double lgammadist(long double x, long double alpha, long double beta) {
  assert(alpha > 0);
  assert(beta > 0);
  return (alpha-1)*log(x) - alpha*log(beta) - x/beta - lgamma(alpha);
}

/* This definition of gammavariate is from Python code in
 * the Python random module.
 */

long double gammavariate(long double alpha, long double beta) {

  assert(alpha > 0);
  assert(beta > 0);

  if (alpha > 1.0) {
    
    /* Uses R.C.H. Cheng, "The generation of Gamma variables with
       non-integral shape parameters", Applied Statistics, (1977), 26,
       No. 1, p71-74 */

    long double ainv = sqrt(2.0 * alpha - 1.0);
    long double bbb = alpha - log(4.0);
    long double ccc = alpha + ainv;
    
    while (1) {
      long double u1 = mt_genrand_real3();
      if (u1 > 1e-7  || u1 < 0.9999999) {
	long double u2 = 1.0 - mt_genrand_real3();
	long double v = log(u1/(1.0-u1))/ainv;
	long double x = alpha*exp(v);
	long double z = u1*u1*u2;
	long double r = bbb+ccc*v-x;
	if (r + (1.0+log(4.5)) - 4.5*z >= 0.0 || r >= log(z))
	  return x * beta;
      }
    }
  }
  else if (alpha == 1.0) {
    long double u = mt_genrand_real3();
    while (u <= 1e-7)
      u = mt_genrand_real3();
    return -log(u) * beta;
  }
  else { 
    /* alpha is between 0 and 1 (exclusive) 
       Uses ALGORITHM GS of Statistical Computing - Kennedy & Gentle */
    
    while (1) {
      long double u = mt_genrand_real3();
      long double b = (exp(1) + alpha)/exp(1);
      long double p = b*u;
      long double x = (p <= 1.0) ? pow(p, 1.0/alpha) : -log((b-p)/alpha);
      long double u1 = mt_genrand_real3();
      if (! (((p <= 1.0) && (u1 > exp(-x))) ||
	     ((p > 1.0)  &&  (u1 > pow(x, alpha - 1.0)))))
	return x * beta;
    }
  }
}

/* betadist() returns the probability density of x under a Beta(alpha,beta)
 * distribution.
 */

long double betadist(long double x, long double alpha, long double beta) {
  assert(x >= 0);
  assert(x <= 1);
  assert(alpha > 0);
  assert(beta > 0);
  return pow(x,alpha-1)*pow(1-x,beta-1)*tgamma(alpha+beta)/(tgamma(alpha)*tgamma(beta));
}

/* lbetadist() returns the log probability density of x under a Beta(alpha,beta)
 * distribution.
 */

long double lbetadist(long double x, long double alpha, long double beta) {
  assert(x > 0);
  assert(x < 1);
  assert(alpha > 0);
  assert(beta > 0);
  return (alpha-1)*log(x)+(beta-1)*log(1-x)+lgamma(alpha+beta)-lgamma(alpha)-lgamma(beta);
}

/* betavariate() generates a sample from a Beta distribution with
 * parameters alpha and beta.
 *
 * 0 < alpha < 1, 0 < beta < 1, mean is alpha/(alpha+beta)
 */

long double betavariate(long double alpha, long double beta) {
  long double x = gammavariate(alpha, 1);
  long double y = gammavariate(beta, 1);
  return x/(x+y);
}

#ifdef GAMMATEST
#include <stdio.h>

int main(int argc, char **argv) {
  int iteration, niterations = 1000;

  for (iteration = 0; iteration < niterations; ++iteration) {
    long double alpha = 100*mt_genrand_real3();
    long double gv = gammavariate(alpha, 1);
    long double pgv = gammadist(gv, alpha, 1);
    long double pgvl = exp(lgammadist(gv, alpha, 1));
    fprintf(stderr, "iteration = %d, gammavariate(%lg,1) = %lg, gammadist(%lg,%lg,1) = %lg, exp(lgammadist(%lg,%lg,1) = %lg\n",
	    iteration, alpha, gv, gv, alpha, pgv, gv, alpha, pgvl);
  }
  return 0;
}

#endif /* GAMMATEST */


/* Other routines I tried, but which weren't as good as the ones above */

#if 0

/*! gammavariate() returns samples from a Gamma distribution
 *! where alpha is the shape parameter and beta is the scale 
 *! parameter, using the algorithm described on p. 94 of 
 *! Gentle (1998) Random Number Generation and Monte Carlo Methods, 
 *! Springer.
 */

long double gammavariate(long double alpha) {

  assert(alpha > 0); 
  
  if (alpha > 1.0) {
    while (1) {
      long double u1 = mt_genrand_real3();
      long double u2 = mt_genrand_real3();
      long double v = (alpha - 1/(6*alpha))*u1/(alpha-1)*u2;
      if (2*(u2-1)/(alpha-1) + v + 1/v <= 2 
         || 2*log(u2)/(alpha-1) - log(v) + v <= 1)
	return (alpha-1)*v;
    }
  } else if (alpha < 1.0) {  
    while (1) {
      long double t = 0.07 + 0.75*sqrt(1-alpha);
      long double b = alpha + exp(-t)*alpha/t;
      long double u1 = mt_genrand_real3();
      long double u2 = mt_genrand_real3();
      long double v = b*u1;
      if (v <= 1) {
	long double x = t*pow(v, 1/alpha);
	if (u2 <= (2 - x)/(2 + x))
	  return x;
	if (u2 <= exp(-x))
	  return x;
      }
      else {
	long double x = log(t*(b-v)/alpha);
	long double y = x/t;
	if (u2*(alpha + y*(1-alpha)) <= 1)
	  return x;
	if (u2 <= pow(y,alpha-1))
	  return x;
      }
    }
  }
  else  
    return -log(mt_genrand_real3());
} 


/*! gammavariate() returns a deviate distributed as a gamma
 *! distribution of order alpha, beta, i.e., a waiting time to the alpha'th
 *! event in a Poisson process of unit mean.
 *!
 *! Code from Numerical Recipes
 */

long double nr_gammavariate(long double ia) {
  int j;
  long double am,e,s,v1,v2,x,y;
  assert(ia > 0);
  if (ia < 10) { 
    x=1.0; 
    for (j=1;j<=ia;j++) 
      x *= mt_genrand_real3();
    x = -log(x);
  } else { 
    do {
      do {
	do { 
	  v1=mt_genrand_real3();
	  v2=2.0*mt_genrand_real3()-1.0;
	} while (v1*v1+v2*v2 > 1.0); 
	y=v2/v1;
	am=ia-1;
	s=sqrt(2.0*am+1.0);
	x=s*y+am;
      } while (x <= 0.0);
      e=(1.0+y*y)*exp(am*log(x/am)-s*y);
    } while (mt_genrand_real3() > e);
  }
  return x;
} 

#endif
