#include "m.h"

#include <iostream>
#include <gtest/gtest.h>
#include <cassert>

using namespace std;

class MTest : public testing::Test {
 public:
  MTest() {}
 protected:
  virtual void SetUp() { }
  virtual void TearDown() { }
};

TEST_F(MTest, Densities) {
  double px1 = Md::log_gaussian_density(1.0, 0.0, 1.0);
  double px2 = Md::log_gaussian_density(-1.0, 0.0, 1.0);
  double py1 = Md::log_laplace_density(1.0, 0.0, 1.0);
  double py2 = Md::log_laplace_density(1.0, 0.0, 1.0);
  double pz1 = Md::log_triangle_density(1.0, -2.0, 2.0, 0.0);
  double pz2 = Md::log_triangle_density(1.0, -2.0, 2.0, 0.0);
  cerr << px1 << " " << py1 << " " << pz2 << endl;
  EXPECT_FLOAT_EQ(px1, px2);
  EXPECT_FLOAT_EQ(py1, py2);
  EXPECT_FLOAT_EQ(pz1, pz2);
  double b1 = Md::log_bivariate_gaussian_density(1.0, -1.0, 0.0, 0.0, 1.0, 1.0, -0.8);
  double b2 = Md::log_bivariate_gaussian_density(-1.0, 1.0, 0.0, 0.0, 1.0, 1.0, -0.8);
  cerr << b1 << " " << b2 << endl;
}

TEST_F(MTest, Poisson) {
  double prev = 1.0;
  double tot = 0;
  for (int i = 0; i < 10; ++i) {
    double p = Md::log_poisson(i, 0.99);
    cerr << "p(i=" << i << ") = " << exp(p) << endl;
    EXPECT_LT(p, prev);
    tot += exp(p);
    prev = p;
  }
  cerr << "  tot=" << tot << endl;
  EXPECT_LE(tot, 1.0);
}

TEST_F(MTest, YuleSimon) {
  double prev = 1.0;
  double tot = 0;
  for (int i = 0; i < 10; ++i) {
    double p = Md::log_yule_simon(i, 1.0);
    cerr << "p(i=" << i << ") = " << exp(p) << endl;
    EXPECT_LT(p, prev);
    tot += exp(p);
    prev = p;
  }
  cerr << "  tot=" << tot << endl;
  EXPECT_LE(tot, 1.0);
}

TEST_F(MTest, LogGeometric) {
  double prev = 1.0;
  double tot = 0;
  for (int i = 0; i < 10; ++i) {
    double p = Md::log_geometric(i, 0.5);
    cerr << "p(i=" << i << ") = " << exp(p) << endl;
    EXPECT_LT(p, prev);
    tot += exp(p);
    prev = p;
  }
  cerr << "  tot=" << tot << endl;
  EXPECT_LE(tot, 1.0);
}

TEST_F(MTest, GeneralizedFactorial) {
  for (double i = 0.3; i < 10000; i += 0.4) {
    double a = Md::log_generalized_factorial(1.0, i);
    double b = lgamma(1.0 + i);
    EXPECT_FLOAT_EQ(a,b);
  }
  double gf_3_6 = 3.0 * 4.0 * 5.0 * 6.0 * 7.0 * 8.0;
  EXPECT_FLOAT_EQ(Md::log_generalized_factorial(3.0, 6.0), std::log(gf_3_6));
  double gf_314_6 = 3.14 * 4.14 * 5.14 * 6.14 * 7.14 * 8.14;
  EXPECT_FLOAT_EQ(Md::log_generalized_factorial(3.14, 6.0), std::log(gf_314_6));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

