#define BOOST_TEST_MODULE WeightsTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <sstream>
#include <string>
#include "sparse_vector.h"
#include "fdict.h"

using namespace std;

BOOST_AUTO_TEST_CASE(Dot) {
  SparseVector<double> x;
  SparseVector<double> y;
  x.set_value(1,0.8);
  y.set_value(1,5);
  x.set_value(2,-2);
  y.set_value(2,1);
  x.set_value(3,80);
  BOOST_CHECK_CLOSE(x.dot(y), 2.0, 1e-9);
}

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

BOOST_AUTO_TEST_CASE(Serialization) {
  string arc;
  FD::dict_.clear();
  {
    SparseVector<double> x;
    x.set_value(FD::Convert("Feature1"), 1.0);
    x.set_value(FD::Convert("Pi"), 3.14);
    ostringstream os;
    boost::archive::text_oarchive oa(os);
    oa << x;
    arc = os.str();
  }
  FD::dict_.clear();
  FD::Convert("SomeNewString");
  {
    SparseVector<double> x;
    istringstream is(arc);
    boost::archive::text_iarchive ia(is);
    ia >> x;
    cerr << x << endl;
    BOOST_CHECK_CLOSE(x.get(FD::Convert("Pi")), 3.14, 1e-9);
    BOOST_CHECK_CLOSE(x.get(FD::Convert("Feature1")), 1.0, 1e-9);
  }
}

