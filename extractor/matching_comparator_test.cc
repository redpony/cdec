#include <gtest/gtest.h>

#include "matching.h"
#include "matching_comparator.h"

using namespace ::testing;

namespace {

class MatchingComparatorTest : public Test {
 protected:
  virtual void SetUp() {
    comparator = make_shared<MatchingComparator>(1, 20);
  }

  shared_ptr<MatchingComparator> comparator;
};

TEST_F(MatchingComparatorTest, SmallerSentenceId) {
  vector<int> left_locations{1};
  Matching left(left_locations.begin(), 1, 1);
  vector<int> right_locations{100};
  Matching right(right_locations.begin(), 1, 5);
  EXPECT_EQ(-1, comparator->Compare(left, right, 1, true));
}

TEST_F(MatchingComparatorTest, GreaterSentenceId) {
  vector<int> left_locations{100};
  Matching left(left_locations.begin(), 1, 5);
  vector<int> right_locations{1};
  Matching right(right_locations.begin(), 1, 1);
  EXPECT_EQ(1, comparator->Compare(left, right, 1, true));
}

TEST_F(MatchingComparatorTest, SmalleraXb) {
  vector<int> left_locations{1};
  Matching left(left_locations.begin(), 1, 1);
  vector<int> right_locations{21};
  Matching right(right_locations.begin(), 1, 1);
  // The matching exceeds the max rule span.
  EXPECT_EQ(-1, comparator->Compare(left, right, 1, true));
}

TEST_F(MatchingComparatorTest, EqualaXb) {
  vector<int> left_locations{1};
  Matching left(left_locations.begin(), 1, 1);
  vector<int> lower_right_locations{3};
  Matching right(lower_right_locations.begin(), 1, 1);
  EXPECT_EQ(0, comparator->Compare(left, right, 1, true));

  vector<int> higher_right_locations{20};
  right = Matching(higher_right_locations.begin(), 1, 1);
  EXPECT_EQ(0, comparator->Compare(left, right, 1, true));
}

TEST_F(MatchingComparatorTest, GreateraXb) {
  vector<int> left_locations{1};
  Matching left(left_locations.begin(), 1, 1);
  vector<int> right_locations{2};
  Matching right(right_locations.begin(), 1, 1);
  // The gap between the prefix and the suffix is of size 0, less than the
  // min gap size.
  EXPECT_EQ(1, comparator->Compare(left, right, 1, true));
}

TEST_F(MatchingComparatorTest, SmalleraXbXc) {
  vector<int> left_locations{1, 3};
  Matching left(left_locations.begin(), 2, 1);
  vector<int> right_locations{4, 6};
  // The common part doesn't match.
  Matching right(right_locations.begin(), 2, 1);
  EXPECT_EQ(-1, comparator->Compare(left, right, 1, true));

  // The common part matches, but the rule exceeds the max span.
  vector<int> other_right_locations{3, 21};
  right = Matching(other_right_locations.begin(), 2, 1);
  EXPECT_EQ(-1, comparator->Compare(left, right, 1, true));
}

TEST_F(MatchingComparatorTest, EqualaXbXc) {
  vector<int> left_locations{1, 3};
  Matching left(left_locations.begin(), 2, 1);
  vector<int> right_locations{3, 5};
  // The leftmost possible match.
  Matching right(right_locations.begin(), 2, 1);
  EXPECT_EQ(0, comparator->Compare(left, right, 1, true));

  // The rightmost possible match.
  vector<int> other_right_locations{3, 20};
  right = Matching(other_right_locations.begin(), 2, 1);
  EXPECT_EQ(0, comparator->Compare(left, right, 1, true));
}

TEST_F(MatchingComparatorTest, GreateraXbXc) {
  vector<int> left_locations{1, 4};
  Matching left(left_locations.begin(), 2, 1);
  vector<int> right_locations{3, 5};
  // The common part doesn't match.
  Matching right(right_locations.begin(), 2, 1);
  EXPECT_EQ(1, comparator->Compare(left, right, 1, true));
}

TEST_F(MatchingComparatorTest, SmallerabXcXd) {
  vector<int> left_locations{9, 13};
  Matching left(left_locations.begin(), 2, 1);
  // The suffix doesn't start on the next position.
  vector<int> right_locations{11, 13, 15};
  Matching right(right_locations.begin(), 3, 1);
  EXPECT_EQ(-1, comparator->Compare(left, right, 1, false));

  // The common part doesn't match.
  vector<int> other_right_locations{10, 16, 18};
  right = Matching(other_right_locations.begin(), 3, 1);
  EXPECT_EQ(-1, comparator->Compare(left, right, 1, false));
}

TEST_F(MatchingComparatorTest, EqualabXcXd) {
  vector<int> left_locations{10, 13};
  Matching left(left_locations.begin(), 2, 1);
  vector<int> right_locations{11, 13, 15};
  Matching right(right_locations.begin(), 3, 1);
  EXPECT_EQ(0, comparator->Compare(left, right, 1, false));
}

TEST_F(MatchingComparatorTest, GreaterabXcXd) {
  vector<int> left_locations{9, 15};
  Matching left(left_locations.begin(), 2, 1);
  // The suffix doesn't start on the next position.
  vector<int> right_locations{7, 15, 17};
  Matching right(right_locations.begin(), 3, 1);
  EXPECT_EQ(1, comparator->Compare(left, right, 1, false));

  // The common part doesn't match.
  vector<int> other_right_locations{10, 13, 16};
  right = Matching(other_right_locations.begin(), 3, 1);
  EXPECT_EQ(1, comparator->Compare(left, right, 1, false));
}

}  // namespace
