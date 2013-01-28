#include <gtest/gtest.h>

#include <vector>

#include "matching.h"

using namespace std;

namespace {

TEST(MatchingTest, SameSize) {
  vector<int> positions{1, 2, 3};
  Matching left(positions.begin(), positions.size(), 0);
  Matching right(positions.begin(), positions.size(), 0);
  EXPECT_EQ(positions, left.Merge(right, positions.size()));
}

TEST(MatchingTest, DifferentSize) {
  vector<int> positions{1, 2, 3};
  Matching left(positions.begin(), positions.size() - 1, 0);
  Matching right(positions.begin() + 1, positions.size() - 1, 0);
  vector<int> result = left.Merge(right, positions.size());
}

}  // namespace
