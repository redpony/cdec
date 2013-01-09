#define BOOST_TEST_MODULE WeightsTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include "weights.h"

using namespace std;

BOOST_AUTO_TEST_CASE(Load) {
  vector<weight_t> v;
  Weights::InitFromFile(TEST_DATA "/weights", &v);
  Weights::WriteToFile("-", v);
}
