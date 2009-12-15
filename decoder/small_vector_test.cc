#include "small_vector.h"

#include <gtest/gtest.h>
#include <iostream>
#include <cassert>
#include <vector>

using namespace std;

class SVTest : public testing::Test {
 protected:
  virtual void SetUp() { }
  virtual void TearDown() { }
};
       
TEST_F(SVTest, LargerThan2) {
  SmallVector v;
  SmallVector v2;
  v.push_back(0);
  v.push_back(1);
  v.push_back(2);
  assert(v.size() == 3);
  assert(v[2] == 2);
  assert(v[1] == 1);
  assert(v[0] == 0);
  v2 = v;
  SmallVector copy(v);
  assert(copy.size() == 3);
  assert(copy[0] == 0);
  assert(copy[1] == 1);
  assert(copy[2] == 2);
  assert(copy == v2);
  copy[1] = 99;
  assert(copy != v2);
  assert(v2.size() == 3);
  assert(v2[2] == 2);
  assert(v2[1] == 1);
  assert(v2[0] == 0);
  v2[0] = -2;
  v2[1] = -1;
  v2[2] = 0;
  assert(v2[2] == 0);
  assert(v2[1] == -1);
  assert(v2[0] == -2);
  SmallVector v3(1,1);
  assert(v3[0] == 1);
  v2 = v3;
  assert(v2.size() == 1);
  assert(v2[0] == 1);
  SmallVector v4(10, 1);
  assert(v4.size() == 10);
  assert(v4[5] == 1);
  assert(v4[9] == 1);
  v4 = v;
  assert(v4.size() == 3);
  assert(v4[2] == 2);
  assert(v4[1] == 1);
  assert(v4[0] == 0);
  SmallVector v5(10, 2);
  assert(v5.size() == 10);
  assert(v5[7] == 2);
  assert(v5[0] == 2);
  assert(v.size() == 3);
  v = v5;
  assert(v.size() == 10);
  assert(v[2] == 2);
  assert(v[9] == 2);
  SmallVector cc;
  for (int i = 0; i < 33; ++i)
    cc.push_back(i);
  for (int i = 0; i < 33; ++i)
    assert(cc[i] == i);
  cc.resize(20);
  assert(cc.size() == 20);
  for (int i = 0; i < 20; ++i)
    assert(cc[i] == i);
  cc[0]=-1;
  cc.resize(1, 999);
  assert(cc.size() == 1);
  assert(cc[0] == -1);
  cc.resize(99, 99);
  for (int i = 1; i < 99; ++i) {
    cerr << i << " " << cc[i] << endl;
    assert(cc[i] == 99);
  }
  cc.clear();
  assert(cc.size() == 0);
}

TEST_F(SVTest, Small) {
  SmallVector v;
  SmallVector v1(1,0);
  SmallVector v2(2,10);
  SmallVector v1a(2,0);
  EXPECT_TRUE(v1 != v1a);
  EXPECT_TRUE(v1 == v1);
  EXPECT_EQ(v1[0], 0);
  EXPECT_EQ(v2[1], 10);
  EXPECT_EQ(v2[0], 10);
  ++v2[1];
  --v2[0];
  EXPECT_EQ(v2[0], 9);
  EXPECT_EQ(v2[1], 11);
  SmallVector v3(v2);
  assert(v3[0] == 9);
  assert(v3[1] == 11);
  assert(!v3.empty());
  assert(v3.size() == 2);
  v3.clear();
  assert(v3.empty());
  assert(v3.size() == 0);
  assert(v3 != v2);
  assert(v2 != v3);
  v3 = v2;
  assert(v3 == v2);
  assert(v2 == v3);
  assert(v3[0] == 9);
  assert(v3[1] == 11);
  assert(!v3.empty());
  assert(v3.size() == 2);
  cerr << sizeof(SmallVector) << endl;
  cerr << sizeof(vector<int>) << endl;
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

