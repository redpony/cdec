#include "m.h"

#define BOOST_TEST_MODULE MTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

#include <iostream>
#include <cassert>

using namespace std;

BOOST_AUTO_TEST_CASE(Densities) {
  double px1 = Md::log_gaussian_density(1.0, 0.0, 1.0);
  double px2 = Md::log_gaussian_density(-1.0, 0.0, 1.0);
  double py1 = Md::log_laplace_density(1.0, 0.0, 1.0);
  double py2 = Md::log_laplace_density(1.0, 0.0, 1.0);
  double pz1 = Md::log_triangle_density(1.0, -2.0, 2.0, 0.0);
  double pz2 = Md::log_triangle_density(1.0, -2.0, 2.0, 0.0);
  cerr << px1 << " " << py1 << " " << pz2 << endl;
  BOOST_CHECK_CLOSE(px1, px2, 1e-6);
  BOOST_CHECK_CLOSE(py1, py2, 1e-6);
  BOOST_CHECK_CLOSE(pz1, pz2, 1e-6);
  double b1 = Md::log_bivariate_gaussian_density(1.0, -1.0, 0.0, 0.0, 1.0, 1.0, -0.8);
  double b2 = Md::log_bivariate_gaussian_density(-1.0, 1.0, 0.0, 0.0, 1.0, 1.0, -0.8);
  cerr << b1 << " " << b2 << endl;
}

BOOST_AUTO_TEST_CASE(Poisson) {
  double prev = 1.0;
  double tot = 0;
  for (int i = 0; i < 10; ++i) {
    double p = Md::log_poisson(i, 0.99);
    cerr << "p(i=" << i << ") = " << exp(p) << endl;
    assert(p < prev);
    tot += exp(p);
    prev = p;
  }
  cerr << "  tot=" << tot << endl;
  assert(tot < 1.0);
}

BOOST_AUTO_TEST_CASE(YuleSimon) {
  double prev = 1.0;
  double tot = 0;
  for (int i = 0; i < 10; ++i) {
    double p = Md::log_yule_simon(i, 1.0);
    cerr << "p(i=" << i << ") = " << exp(p) << endl;
    assert(p < prev);
    tot += exp(p);
    prev = p;
  }
  cerr << "  tot=" << tot << endl;
  assert(tot < 1.0);
}

BOOST_AUTO_TEST_CASE(LogGeometric) {
  double prev = 1.0;
  double tot = 0;
  for (int i = 0; i < 10; ++i) {
    double p = Md::log_geometric(i, 0.5);
    cerr << "p(i=" << i << ") = " << exp(p) << endl;
    assert(p < prev);
    tot += exp(p);
    prev = p;
  }
  cerr << "  tot=" << tot << endl;
  assert(tot <= 1.0);
}

BOOST_AUTO_TEST_CASE(GeneralizedFactorial) {
  for (double i = 0.3; i < 10000; i += 0.4) {
    double a = Md::log_generalized_factorial(1.0, i);
    double b = lgamma(1.0 + i);
    BOOST_CHECK_CLOSE(a,b,1e-6);
  }
  double gf_3_6 = 3.0 * 4.0 * 5.0 * 6.0 * 7.0 * 8.0;
  BOOST_CHECK_CLOSE(Md::log_generalized_factorial(3.0, 6.0), std::log(gf_3_6), 1e-6);
  double gf_314_6 = 3.14 * 4.14 * 5.14 * 6.14 * 7.14 * 8.14;
  BOOST_CHECK_CLOSE(Md::log_generalized_factorial(3.14, 6.0), std::log(gf_314_6), 1e-6);
}

