#include <gtest/gtest.h>

#include <memory>

#include "matchings_finder.h"
#include "mocks/mock_suffix_array.h"
#include "phrase_location.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class MatchingsFinderTest : public Test {
 protected:
  virtual void SetUp() {
    suffix_array = make_shared<MockSuffixArray>();
    EXPECT_CALL(*suffix_array, Lookup(0, 10, _, _))
        .Times(1)
        .WillOnce(Return(PhraseLocation(3, 5)));

    matchings_finder = make_shared<MatchingsFinder>(suffix_array);
  }

  shared_ptr<MatchingsFinder> matchings_finder;
  shared_ptr<MockSuffixArray> suffix_array;
};

TEST_F(MatchingsFinderTest, TestFind) {
  PhraseLocation phrase_location(0, 10), expected_result(3, 5);
  EXPECT_EQ(expected_result, matchings_finder->Find(phrase_location, "bla", 2));
}

TEST_F(MatchingsFinderTest, ResizeUnsetRange) {
  EXPECT_CALL(*suffix_array, GetSize()).Times(1).WillOnce(Return(10));

  PhraseLocation phrase_location, expected_result(3, 5);
  EXPECT_EQ(expected_result, matchings_finder->Find(phrase_location, "bla", 2));
  EXPECT_EQ(PhraseLocation(0, 10), phrase_location);
}

} // namespace
} // namespace extractor
