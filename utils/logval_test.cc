#include "logval.h"
#define BOOST_TEST_MODULE LogValTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <iostream>

using namespace std;

BOOST_AUTO_TEST_CASE(Order) {
  LogVal<double> a(-0.3);
  LogVal<double> b(0.3);
  LogVal<double> c(2.4);
  BOOST_CHECK_LT(a,b);
  BOOST_CHECK_LT(b,c);
  BOOST_CHECK_LT(a,c);
  BOOST_CHECK(b >= a);
  BOOST_CHECK(c >= a);
  BOOST_CHECK(c >= b);
  BOOST_CHECK(c >= c);
  BOOST_CHECK(b >= b);
  BOOST_CHECK(a >= a);
}

BOOST_AUTO_TEST_CASE(Negate) {
  LogVal<double> x(-2.4);
  LogVal<double> y(2.4);
  y.negate();
  BOOST_CHECK_CLOSE(x.as_float(),y.as_float(), 1e-6);
}

BOOST_AUTO_TEST_CASE(Inverse) {
  LogVal<double> x(1/2.4);
  LogVal<double> y(2.4);
  BOOST_CHECK_CLOSE(x.as_float(),y.inverse().as_float(), 1e-6);
}

BOOST_AUTO_TEST_CASE(Minus) {
  LogVal<double> x(12);
  LogVal<double> y(2);
  LogVal<double> z1 = x - y;
  LogVal<double> z2 = x;
  z2 -= y;
  BOOST_CHECK_CLOSE(z1.as_float(), z2.as_float(), 1e-6);
  BOOST_CHECK_CLOSE(z1.as_float(), 10.0, 1e-6);
  BOOST_CHECK_CLOSE((y - x).as_float(), -10.0, 1e-6);
}

BOOST_AUTO_TEST_CASE(TestOps) {
  LogVal<double> x(-12.12);
  LogVal<double> y(x);
  cerr << x << endl;
  cerr << (x*y) << endl;
  cerr << (x*y + x) << endl;
  cerr << (x + x*y) << endl;
  cerr << log1p(-0.5) << endl;
  LogVal<double> aa(0.2);
  LogVal<double> bb(-0.3);
  cerr << (aa + bb) << endl;
  cerr << (bb + aa) << endl;
  BOOST_CHECK_CLOSE((aa + bb).as_float(), (bb + aa).as_float(), 1e-6);
  BOOST_CHECK_CLOSE((aa + bb).as_float(), -0.1, 1e-6);
}

BOOST_AUTO_TEST_CASE(TestSizes) {
  cerr << sizeof(LogVal<double>) << endl;
  cerr << sizeof(LogVal<float>) << endl;
  cerr << sizeof(void*) << endl;
}

