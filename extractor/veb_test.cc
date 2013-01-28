#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "veb.h"

using namespace std;

namespace {

class VEBTest : public ::testing::Test {
 protected:
  void VEBSortTester(vector<int> values, int max_value) {
    shared_ptr<VEB> veb = VEB::Create(max_value);
    for (int value: values) {
      veb->Insert(value);
    }

    sort(values.begin(), values.end());
    EXPECT_EQ(values.front(), veb->GetMinimum());
    EXPECT_EQ(values.back(), veb->GetMaximum());
    for (size_t i = 0; i + 1 < values.size(); ++i) {
      EXPECT_EQ(values[i + 1], veb->GetSuccessor(values[i]));
    }
    EXPECT_EQ(-1, veb->GetSuccessor(values.back()));
  }
};

TEST_F(VEBTest, SmallRange) {
  vector<int> values{8, 13, 5, 1, 4, 15, 2, 10, 6, 7};
  VEBSortTester(values, 16);
}

TEST_F(VEBTest, MediumRange) {
  vector<int> values{167, 243, 88, 12, 137, 199, 212, 45, 150, 189};
  VEBSortTester(values, 255);
}

TEST_F(VEBTest, LargeRangeSparse) {
  vector<int> values;
  for (size_t i = 0; i < 100; ++i) {
    values.push_back(i * 1000000);
  }
  VEBSortTester(values, 100000000);
}

TEST_F(VEBTest, LargeRangeDense) {
  vector<int> values;
  for (size_t i = 0; i < 1000000; ++i) {
    values.push_back(i);
  }
  VEBSortTester(values, 1000000);
}

}  // namespace
