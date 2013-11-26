#include "small_vector.h"

#define BOOST_TEST_MODULE svTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <iostream>
#include <vector>

using namespace std;

BOOST_AUTO_TEST_CASE(LargerThan2) {
  SmallVectorInt v;
  SmallVectorInt v2;
  v.push_back(0);
  v.push_back(1);
  v.push_back(2);
  BOOST_CHECK(v.size() == 3);
  BOOST_CHECK(v[2] == 2);
  BOOST_CHECK(v[1] == 1);
  BOOST_CHECK(v[0] == 0);
  v2 = v;
  SmallVectorInt copy(v);
  BOOST_CHECK(copy.size() == 3);
  BOOST_CHECK(copy[0] == 0);
  BOOST_CHECK(copy[1] == 1);
  BOOST_CHECK(copy[2] == 2);
  BOOST_CHECK(copy == v2);
  copy[1] = 99;
  BOOST_CHECK(copy != v2);
  BOOST_CHECK(v2.size() == 3);
  BOOST_CHECK(v2[2] == 2);
  BOOST_CHECK(v2[1] == 1);
  BOOST_CHECK(v2[0] == 0);
  v2[0] = -2;
  v2[1] = -1;
  v2[2] = 0;
  BOOST_CHECK(v2[2] == 0);
  BOOST_CHECK(v2[1] == -1);
  BOOST_CHECK(v2[0] == -2);
  SmallVectorInt v3(1,1);
  BOOST_CHECK(v3[0] == 1);
  v2 = v3;
  BOOST_CHECK(v2.size() == 1);
  BOOST_CHECK(v2[0] == 1);
  SmallVectorInt v4(10, 1);
  BOOST_CHECK(v4.size() == 10);
  BOOST_CHECK(v4[5] == 1);
  BOOST_CHECK(v4[9] == 1);
  v4 = v;
  BOOST_CHECK(v4.size() == 3);
  BOOST_CHECK(v4[2] == 2);
  BOOST_CHECK(v4[1] == 1);
  BOOST_CHECK(v4[0] == 0);
  SmallVectorInt v5(10, 2);
  BOOST_CHECK(v5.size() == 10);
  BOOST_CHECK(v5[7] == 2);
  BOOST_CHECK(v5[0] == 2);
  BOOST_CHECK(v.size() == 3);
  v = v5;
  BOOST_CHECK(v.size() == 10);
  BOOST_CHECK(v[2] == 2);
  BOOST_CHECK(v[9] == 2);
  SmallVectorInt cc;
  for (int i = 0; i < 33; ++i)
    cc.push_back(i);
  for (int i = 0; i < 33; ++i)
    BOOST_CHECK(cc[i] == i);
  cc.resize(20);
  BOOST_CHECK(cc.size() == 20);
  for (int i = 0; i < 20; ++i)
    BOOST_CHECK(cc[i] == i);
  cc[0]=-1;
  cc.resize(1, 999);
  BOOST_CHECK(cc.size() == 1);
  BOOST_CHECK(cc[0] == -1);
  cc.resize(99, 99);
  for (int i = 1; i < 99; ++i) {
    cerr << i << " " << cc[i] << endl;
    BOOST_CHECK(cc[i] == 99);
  }
  cc.clear();
  BOOST_CHECK(cc.size() == 0);
}

BOOST_AUTO_TEST_CASE(SwapSV) {
  SmallVectorInt v;
  SmallVectorInt v2(2, 10);
  SmallVectorInt v3(2, 10);
  BOOST_CHECK(v2 == v3);
  BOOST_CHECK(v != v3);
  v.swap(v2);
  BOOST_CHECK(v == v3);
  SmallVectorInt v4;
  BOOST_CHECK(v4 == v2);
}

BOOST_AUTO_TEST_CASE(Small) {
  SmallVectorInt v;
  SmallVectorInt v1(1,0);
  SmallVectorInt v2(2,10);
  SmallVectorInt v1a(2,0);
  BOOST_CHECK(v1 != v1a);
  BOOST_CHECK(v1 == v1);
  BOOST_CHECK_EQUAL(v1[0], 0);
  BOOST_CHECK_EQUAL(v2[1], 10);
  BOOST_CHECK_EQUAL(v2[0], 10);
  ++v2[1];
  --v2[0];
  BOOST_CHECK_EQUAL(v2[0], 9);
  BOOST_CHECK_EQUAL(v2[1], 11);
  SmallVectorInt v3(v2);
  BOOST_CHECK(v3[0] == 9);
  BOOST_CHECK(v3[1] == 11);
  BOOST_CHECK(!v3.empty());
  BOOST_CHECK(v3.size() == 2);
  v3.clear();
  BOOST_CHECK(v3.empty());
  BOOST_CHECK(v3.size() == 0);
  BOOST_CHECK(v3 != v2);
  BOOST_CHECK(v2 != v3);
  v3 = v2;
  BOOST_CHECK(v3 == v2);
  BOOST_CHECK(v2 == v3);
  BOOST_CHECK(v3[0] == 9);
  BOOST_CHECK(v3[1] == 11);
  BOOST_CHECK(!v3.empty());
  BOOST_CHECK(v3.size() == 2);
  cerr << sizeof(SmallVectorInt) << endl;
  cerr << sizeof(vector<int>) << endl;
}
