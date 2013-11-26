#define BOOST_TEST_MODULE WeightsTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include "sparse_vector.h"

using namespace std;

BOOST_AUTO_TEST_CASE(Equality) {
  SparseVector<double> x;
  SparseVector<double> y;
  x.set_value(1,-1);
  y.set_value(1,-1);
  BOOST_CHECK(x == y);
}

BOOST_AUTO_TEST_CASE(Division) {
  SparseVector<double> x;
  SparseVector<double> y;
  x.set_value(1,1);
  y.set_value(1,-1);
  BOOST_CHECK(!(x == y));
  x /= -1;
  BOOST_CHECK(x == y);
}
