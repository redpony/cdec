#include "dict.h"

#include "fdict.h"

#include <iostream>
#define BOOST_TEST_MODULE CrpTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <cassert>

using namespace std;

BOOST_AUTO_TEST_CASE(Convert) {
  Dict d;
  WordID a = d.Convert("foo");
  WordID b = d.Convert("bar");
  std::string x = "foo";
  WordID c = d.Convert(x);
  assert(a != b);
  BOOST_CHECK_EQUAL(a, c);
  BOOST_CHECK_EQUAL(d.Convert(a), "foo");
  BOOST_CHECK_EQUAL(d.Convert(b), "bar");
}

BOOST_AUTO_TEST_CASE(FDictTest) {
  int fid = FD::Convert("First");
  assert(fid > 0);
  BOOST_CHECK_EQUAL(FD::Convert(fid), "First");
  string x = FD::Escape("=");
  cerr << x << endl;
  assert(x != "=");
  x = FD::Escape(";");
  cerr << x << endl;
  assert(x != ";");
}

