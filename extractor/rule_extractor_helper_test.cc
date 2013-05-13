#include <gtest/gtest.h>

#include <memory>

#include "mocks/mock_alignment.h"
#include "mocks/mock_data_array.h"
#include "rule_extractor_helper.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class RuleExtractorHelperTest : public Test {
 protected:
  virtual void SetUp() {
    source_data_array = make_shared<MockDataArray>();
    EXPECT_CALL(*source_data_array, GetSentenceLength(_))
        .WillRepeatedly(Return(12));

    target_data_array = make_shared<MockDataArray>();
    EXPECT_CALL(*target_data_array, GetSentenceLength(_))
        .WillRepeatedly(Return(12));

    vector<pair<int, int>> links = {
      make_pair(0, 0), make_pair(0, 1), make_pair(2, 2), make_pair(3, 1)
    };
    alignment = make_shared<MockAlignment>();
    EXPECT_CALL(*alignment, GetLinks(_)).WillRepeatedly(Return(links));
  }

  shared_ptr<MockDataArray> source_data_array;
  shared_ptr<MockDataArray> target_data_array;
  shared_ptr<MockAlignment> alignment;
  shared_ptr<RuleExtractorHelper> helper;
};

TEST_F(RuleExtractorHelperTest, TestGetLinksSpans) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(4));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(3));

  vector<int> source_low, source_high, target_low, target_high;
  helper->GetLinksSpans(source_low, source_high, target_low, target_high, 0);

  vector<int> expected_source_low = {0, -1, 2, 1};
  EXPECT_EQ(expected_source_low, source_low);
  vector<int> expected_source_high = {2, -1, 3, 2};
  EXPECT_EQ(expected_source_high, source_high);
  vector<int> expected_target_low = {0, 0, 2};
  EXPECT_EQ(expected_target_low, target_low);
  vector<int> expected_target_high = {1, 4, 3};
  EXPECT_EQ(expected_target_high, target_high);
}

TEST_F(RuleExtractorHelperTest, TestCheckAlignedFalse) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, false, false, true);
  EXPECT_CALL(*source_data_array, GetSentenceId(_)).Times(0);
  EXPECT_CALL(*source_data_array, GetSentenceStart(_)).Times(0);

  vector<int> matching, chunklen, source_low;
  EXPECT_TRUE(helper->CheckAlignedTerminals(matching, chunklen,
                                            source_low, 10));
}

TEST_F(RuleExtractorHelperTest, TestCheckAlignedTerminal) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, false, true);

  vector<int> matching = {10, 12};
  vector<int> chunklen = {1, 3};
  vector<int> source_low = {-1, 1, -1, 3, -1};
  EXPECT_TRUE(helper->CheckAlignedTerminals(matching, chunklen,
                                            source_low, 10));
  source_low = {-1, 1, -1, -1, -1};
  EXPECT_FALSE(helper->CheckAlignedTerminals(matching, chunklen,
                                             source_low, 10));
}

TEST_F(RuleExtractorHelperTest, TestCheckAlignedChunks) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);

  vector<int> matching = {10, 12};
  vector<int> chunklen = {1, 3};
  vector<int> source_low = {2, 1, -1, 3, -1};
  EXPECT_TRUE(helper->CheckAlignedTerminals(matching, chunklen,
                                            source_low, 10));
  source_low = {-1, 1, -1, 3, -1};
  EXPECT_FALSE(helper->CheckAlignedTerminals(matching, chunklen,
                                             source_low, 10));
  source_low = {2, 1, -1, -1, -1};
  EXPECT_FALSE(helper->CheckAlignedTerminals(matching, chunklen,
                                             source_low, 10));
}


TEST_F(RuleExtractorHelperTest, TestCheckTightPhrasesFalse) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, false);
  EXPECT_CALL(*source_data_array, GetSentenceId(_)).Times(0);
  EXPECT_CALL(*source_data_array, GetSentenceStart(_)).Times(0);

  vector<int> matching, chunklen, source_low;
  EXPECT_TRUE(helper->CheckTightPhrases(matching, chunklen, source_low, 10));
}

TEST_F(RuleExtractorHelperTest, TestCheckTightPhrases) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);

  vector<int> matching = {10, 14, 18};
  vector<int> chunklen = {2, 3, 1};
  // No missing links.
  vector<int> source_low = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_TRUE(helper->CheckTightPhrases(matching, chunklen, source_low, 10));

  // Missing link at the beginning or ending of a gap.
  source_low = {0, 1, -1, 3, 4, 5, 6, 7, 8};
  EXPECT_FALSE(helper->CheckTightPhrases(matching, chunklen, source_low, 10));
  source_low = {0, 1, 2, -1, 4, 5, 6, 7, 8};
  EXPECT_FALSE(helper->CheckTightPhrases(matching, chunklen, source_low, 10));
  source_low = {0, 1, 2, 3, 4, 5, 6, -1, 8};
  EXPECT_FALSE(helper->CheckTightPhrases(matching, chunklen, source_low, 10));

  // Missing link inside the gap.
  chunklen = {1, 3, 1};
  source_low = {0, 1, -1, 3, 4, 5, 6, 7, 8};
  EXPECT_TRUE(helper->CheckTightPhrases(matching, chunklen, source_low, 10));
}

TEST_F(RuleExtractorHelperTest, TestFindFixPointBadEdgeCase) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);

  vector<int> source_low = {0, -1, 2};
  vector<int> source_high = {1, -1, 3};
  vector<int> target_low = {0, -1, 2};
  vector<int> target_high = {1, -1, 3};
  int source_phrase_low = 1, source_phrase_high = 2;
  int source_back_low, source_back_high;
  int target_phrase_low = -1, target_phrase_high = 1;

  // This should be in fact true. See comment about the inherited bug.
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 0, 0,
                                    0, false, false, false));
}

TEST_F(RuleExtractorHelperTest, TestFindFixPointTargetSentenceOutOfBounds) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(3));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(3));

  vector<int> source_low = {0, 0, 2};
  vector<int> source_high = {1, 2, 3};
  vector<int> target_low = {0, 1, 2};
  vector<int> target_high = {2, 2, 3};
  int source_phrase_low = 1, source_phrase_high = 2;
  int source_back_low, source_back_high;
  int target_phrase_low = 1, target_phrase_high = 2;

  // Extend out of sentence to left.
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 2, 2,
                                    0, false, false, false));
  source_low = {0, 1, 2};
  source_high = {1, 3, 3};
  target_low = {0, 1, 1};
  target_high = {1, 2, 3};
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 2, 2,
                                    0, false, false, false));
}

TEST_F(RuleExtractorHelperTest, TestFindFixPointTargetTooWide) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 5, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));

  vector<int> source_low = {0, 0, 0, 0, 0, 0, 0};
  vector<int> source_high = {7, 7, 7, 7, 7, 7, 7};
  vector<int> target_low = {0, -1, -1, -1, -1, -1, 0};
  vector<int> target_high = {7, -1, -1, -1, -1, -1, 7};
  int source_phrase_low = 2, source_phrase_high = 5;
  int source_back_low, source_back_high;
  int target_phrase_low = -1, target_phrase_high = -1;

  // Projection is too wide.
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 1, 1,
                                    0, false, false, false));
}

TEST_F(RuleExtractorHelperTest, TestFindFixPoint) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));

  vector<int> source_low = {1, 1, 1, 3, 4, 5, 5};
  vector<int> source_high = {2, 2, 3, 4, 6, 6, 6};
  vector<int> target_low = {-1, 0, 2, 3, 4, 4, -1};
  vector<int> target_high = {-1, 3, 3, 4, 5, 7, -1};
  int source_phrase_low = 2, source_phrase_high = 5;
  int source_back_low, source_back_high;
  int target_phrase_low = 2, target_phrase_high = 5;

  EXPECT_TRUE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                   source_low, source_high, target_phrase_low,
                                   target_phrase_high, target_low, target_high,
                                   source_back_low, source_back_high, 1, 1, 1,
                                   2, true, true, false));
  EXPECT_EQ(1, target_phrase_low);
  EXPECT_EQ(6, target_phrase_high);
  EXPECT_EQ(0, source_back_low);
  EXPECT_EQ(7, source_back_high);

  source_low = {0, -1, 1, 3, 4, -1, 6};
  source_high = {1, -1, 3, 4, 6, -1, 7};
  target_low = {0, 2, 2, 3, 4, 4, 6};
  target_high = {1, 3, 3, 4, 5, 5, 7};
  source_phrase_low = 2, source_phrase_high = 5;
  target_phrase_low = -1, target_phrase_high = -1;
  EXPECT_TRUE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                   source_low, source_high, target_phrase_low,
                                   target_phrase_high, target_low, target_high,
                                   source_back_low, source_back_high, 1, 1, 1,
                                   2, true, true, false));
  EXPECT_EQ(1, target_phrase_low);
  EXPECT_EQ(6, target_phrase_high);
  EXPECT_EQ(2, source_back_low);
  EXPECT_EQ(5, source_back_high);
}

TEST_F(RuleExtractorHelperTest, TestFindFixPointExtensionsNotAllowed) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(3));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(3));

  vector<int> source_low = {0, 0, 2};
  vector<int> source_high = {1, 2, 3};
  vector<int> target_low = {0, 1, 2};
  vector<int> target_high = {2, 2, 3};
  int source_phrase_low = 1, source_phrase_high = 2;
  int source_back_low, source_back_high;
  int target_phrase_low = -1, target_phrase_high = -1;

  // Extension on the left side not allowed.
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 1, 1,
                                    1, false, true, false));
  // Extension on the left side is allowed, but we can't add anymore X.
  target_phrase_low = -1, target_phrase_high = -1;
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 1, 1,
                                    0, true, true, false));
  source_low = {0, 1, 2};
  source_high = {1, 3, 3};
  target_low = {0, 1, 1};
  target_high = {1, 2, 3};
  // Extension on the right side not allowed.
  target_phrase_low = -1, target_phrase_high = -1;
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 1, 1,
                                    1, true, false, false));
  // Extension on the right side is allowed, but we can't add anymore X.
  target_phrase_low = -1, target_phrase_high = -1;
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 1, 1,
                                    0, true, true, false));
}

TEST_F(RuleExtractorHelperTest, TestFindFixPointSourceSentenceOutOfBounds) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(3));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(3));

  vector<int> source_low = {0, 0, 2};
  vector<int> source_high = {1, 2, 3};
  vector<int> target_low = {0, 1, 2};
  vector<int> target_high = {2, 2, 3};
  int source_phrase_low = 1, source_phrase_high = 2;
  int source_back_low, source_back_high;
  int target_phrase_low = 1, target_phrase_high = 2;
  // Extend out of sentence to left.
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 2, 1,
                                    1, true, true, false));
  source_low = {0, 1, 2};
  source_high = {1, 3, 3};
  target_low = {0, 1, 1};
  target_high = {1, 2, 3};
  target_phrase_low = 1, target_phrase_high = 2;
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 2, 1,
                                    1, true, true, false));
}

TEST_F(RuleExtractorHelperTest, TestFindFixPointTargetSourceWide) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 5, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));

  vector<int> source_low = {2, -1, 2, 3, 4, -1, 4};
  vector<int> source_high = {3, -1, 3, 4, 5, -1, 5};
  vector<int> target_low = {-1, -1, 0, 3, 4, -1, -1};
  vector<int> target_high = {-1, -1, 3, 4, 7, -1, -1};
  int source_phrase_low = 2, source_phrase_high = 5;
  int source_back_low, source_back_high;
  int target_phrase_low = -1, target_phrase_high = -1;

  // Second projection (on source side) is too wide.
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 1, 1,
                                    2, true, true, false));
}

TEST_F(RuleExtractorHelperTest, TestFindFixPointArbitraryExpansion) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 20, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(11));

  vector<int> source_low = {1, 1, 2, 3, 4, 5, 6, 7, 7, 8, 9};
  vector<int> source_high = {2, 3, 4, 5, 5, 6, 7, 8, 9, 10, 10};
  vector<int> target_low = {-1, 0, 1, 2, 3, 5, 6, 7, 8, 9, -1};
  vector<int> target_high = {-1, 2, 3, 4, 5, 6, 8, 9, 10, 11, -1};
  int source_phrase_low = 4, source_phrase_high = 7;
  int source_back_low, source_back_high;
  int target_phrase_low = -1, target_phrase_high = -1;
  EXPECT_FALSE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                    source_low, source_high, target_phrase_low,
                                    target_phrase_high, target_low, target_high,
                                    source_back_low, source_back_high, 0, 1, 1,
                                    10, true, true, false));

  source_phrase_low = 4, source_phrase_high = 7;
  target_phrase_low = -1, target_phrase_high = -1;
  EXPECT_TRUE(helper->FindFixPoint(source_phrase_low, source_phrase_high,
                                   source_low, source_high, target_phrase_low,
                                   target_phrase_high, target_low, target_high,
                                   source_back_low, source_back_high, 0, 1, 1,
                                   10, true, true, true));
}

TEST_F(RuleExtractorHelperTest, TestGetGapOrder) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);

  vector<pair<int, int>> gaps =
      {make_pair(0, 3), make_pair(5, 8), make_pair(11, 12), make_pair(15, 17)};
  vector<int> expected_gap_order = {0, 1, 2, 3};
  EXPECT_EQ(expected_gap_order, helper->GetGapOrder(gaps));

  gaps = {make_pair(15, 17), make_pair(8, 9), make_pair(5, 6), make_pair(0, 3)};
  expected_gap_order = {3, 2, 1, 0};
  EXPECT_EQ(expected_gap_order, helper->GetGapOrder(gaps));

  gaps = {make_pair(8, 9), make_pair(5, 6), make_pair(0, 3), make_pair(15, 17)};
  expected_gap_order = {2, 1, 0, 3};
  EXPECT_EQ(expected_gap_order, helper->GetGapOrder(gaps));
}

TEST_F(RuleExtractorHelperTest, TestGetGapsExceedNumSymbols) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));

  bool met_constraints = true;
  vector<int> source_low = {1, 1, 2, 3, 4, 5, 6};
  vector<int> source_high = {2, 2, 3, 4, 5, 6, 7};
  vector<int> target_low = {-1, 0, 2, 3, 4, 5, 6};
  vector<int> target_high = {-1, 2, 3, 4, 5, 6, 7};
  int source_phrase_low = 1, source_phrase_high = 6;
  int source_back_low = 0, source_back_high = 6;
  vector<int> matching = {11, 13, 15};
  vector<int> chunklen = {1, 1, 1};
  vector<pair<int, int>> source_gaps, target_gaps;
  int num_symbols = 5;
  EXPECT_FALSE(helper->GetGaps(source_gaps, target_gaps, matching, chunklen,
                               source_low, source_high, target_low, target_high,
                               source_phrase_low, source_phrase_high,
                               source_back_low, source_back_high, 5, 10,
                               num_symbols, met_constraints));

  source_low = {0, 1, 2, 3, 4, 5, 5};
  source_high = {1, 2, 3, 4, 5, 6, 6};
  target_low = {0, 1, 2, 3, 4, 5, -1};
  target_high = {1, 2, 3, 4, 5, 7, -1};
  source_phrase_low = 1, source_phrase_high = 6;
  source_back_low = 1, source_back_high = 7;
  num_symbols = 5;
  EXPECT_FALSE(helper->GetGaps(source_gaps, target_gaps, matching, chunklen,
                               source_low, source_high, target_low, target_high,
                               source_phrase_low, source_phrase_high,
                               source_back_low, source_back_high, 5, 10,
                               num_symbols, met_constraints));
}

TEST_F(RuleExtractorHelperTest, TestGetGapsExtensionsNotTight) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 7, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));

  bool met_constraints = true;
  vector<int> source_low = {-1, 1, 2, 3, 4, 5, -1};
  vector<int> source_high = {-1, 2, 3, 4, 5, 6, -1};
  vector<int> target_low = {-1, 1, 2, 3, 4, 5, -1};
  vector<int> target_high = {-1, 2, 3, 4, 5, 6, -1};
  int source_phrase_low = 1, source_phrase_high = 6;
  int source_back_low = 0, source_back_high = 6;
  vector<int> matching = {11, 13, 15};
  vector<int> chunklen = {1, 1, 1};
  vector<pair<int, int>> source_gaps, target_gaps;
  int num_symbols = 5;
  EXPECT_FALSE(helper->GetGaps(source_gaps, target_gaps, matching, chunklen,
                               source_low, source_high, target_low, target_high,
                               source_phrase_low, source_phrase_high,
                               source_back_low, source_back_high, 5, 10,
                               num_symbols, met_constraints));

  source_phrase_low = 1, source_phrase_high = 6;
  source_back_low = 1, source_back_high = 7;
  num_symbols = 5;
  EXPECT_FALSE(helper->GetGaps(source_gaps, target_gaps, matching, chunklen,
                               source_low, source_high, target_low, target_high,
                               source_phrase_low, source_phrase_high,
                               source_back_low, source_back_high, 5, 10,
                               num_symbols, met_constraints));
}

TEST_F(RuleExtractorHelperTest, TestGetGapsNotTightExtremities) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 7, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));

  bool met_constraints = true;
  vector<int> source_low = {-1, -1, 2, 3, 4, 5, 6};
  vector<int> source_high = {-1, -1, 3, 4, 5, 6, 7};
  vector<int> target_low = {-1, -1, 2, 3, 4, 5, 6};
  vector<int> target_high = {-1, -1, 3, 4, 5, 6, 7};
  int source_phrase_low = 1, source_phrase_high = 6;
  int source_back_low = 1, source_back_high = 6;
  vector<int> matching = {11, 13, 15};
  vector<int> chunklen = {1, 1, 1};
  vector<pair<int, int>> source_gaps, target_gaps;
  int num_symbols = 5;
  EXPECT_TRUE(helper->GetGaps(source_gaps, target_gaps, matching, chunklen,
                              source_low, source_high, target_low, target_high,
                              source_phrase_low, source_phrase_high,
                              source_back_low, source_back_high, 5, 10,
                              num_symbols, met_constraints));
  EXPECT_FALSE(met_constraints);
  vector<pair<int, int>> expected_gaps = {make_pair(2, 3), make_pair(4, 5)};
  EXPECT_EQ(expected_gaps, source_gaps);
  EXPECT_EQ(expected_gaps, target_gaps);

  source_low = {-1, 1, 2, 3, 4, -1, 6};
  source_high = {-1, 2, 3, 4, 5, -1, 7};
  target_low = {-1, 1, 2, 3, 4, -1, 6};
  target_high = {-1, 2, 3, 4, 5, -1, 7};
  met_constraints = true;
  source_gaps.clear();
  target_gaps.clear();
  EXPECT_TRUE(helper->GetGaps(source_gaps, target_gaps, matching, chunklen,
                              source_low, source_high, target_low, target_high,
                              source_phrase_low, source_phrase_high,
                              source_back_low, source_back_high, 5, 10,
                              num_symbols, met_constraints));
  EXPECT_FALSE(met_constraints);
  EXPECT_EQ(expected_gaps, source_gaps);
  EXPECT_EQ(expected_gaps, target_gaps);
}

TEST_F(RuleExtractorHelperTest, TestGetGapsWithExtensions) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));

  bool met_constraints = true;
  vector<int> source_low = {-1, 5, 2, 3, 4, 1, -1};
  vector<int> source_high = {-1, 6, 3, 4, 5, 2, -1};
  vector<int> target_low = {-1, 5, 2, 3, 4, 1, -1};
  vector<int> target_high = {-1, 6, 3, 4, 5, 2, -1};
  int source_phrase_low = 2, source_phrase_high = 5;
  int source_back_low = 1, source_back_high = 6;
  vector<int> matching = {12, 14};
  vector<int> chunklen = {1, 1};
  vector<pair<int, int>> source_gaps, target_gaps;
  int num_symbols = 3;
  EXPECT_TRUE(helper->GetGaps(source_gaps, target_gaps, matching, chunklen,
                              source_low, source_high, target_low, target_high,
                              source_phrase_low, source_phrase_high,
                              source_back_low, source_back_high, 5, 10,
                              num_symbols, met_constraints));
  vector<pair<int, int>> expected_source_gaps = {
    make_pair(1, 2), make_pair(3, 4), make_pair(5, 6)
  };
  EXPECT_EQ(expected_source_gaps, source_gaps);
  vector<pair<int, int>> expected_target_gaps = {
    make_pair(5, 6), make_pair(3, 4), make_pair(1, 2)
  };
  EXPECT_EQ(expected_target_gaps, target_gaps);
}

TEST_F(RuleExtractorHelperTest, TestGetGaps) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));

  bool met_constraints = true;
  vector<int> source_low = {-1, 1, 4, 3, 2, 5, -1};
  vector<int> source_high = {-1, 2, 5, 4, 3, 6, -1};
  vector<int> target_low = {-1, 1, 4, 3, 2, 5, -1};
  vector<int> target_high = {-1, 2, 5, 4, 3, 6, -1};
  int source_phrase_low = 1, source_phrase_high = 6;
  int source_back_low = 1, source_back_high = 6;
  vector<int> matching = {11, 13, 15};
  vector<int> chunklen = {1, 1, 1};
  vector<pair<int, int>> source_gaps, target_gaps;
  int num_symbols = 5;
  EXPECT_TRUE(helper->GetGaps(source_gaps, target_gaps, matching, chunklen,
                              source_low, source_high, target_low, target_high,
                              source_phrase_low, source_phrase_high,
                              source_back_low, source_back_high, 5, 10,
                              num_symbols, met_constraints));
  vector<pair<int, int>> expected_source_gaps = {
    make_pair(2, 3), make_pair(4, 5)
  };
  EXPECT_EQ(expected_source_gaps, source_gaps);
  vector<pair<int, int>> expected_target_gaps = {
    make_pair(4, 5), make_pair(2, 3)
  };
  EXPECT_EQ(expected_target_gaps, target_gaps);
}

TEST_F(RuleExtractorHelperTest, TestGetGapIntegrityChecksFailed) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);
  EXPECT_CALL(*source_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));
  EXPECT_CALL(*target_data_array, GetSentenceLength(_))
      .WillRepeatedly(Return(7));

  bool met_constraints = true;
  vector<int> source_low = {-1, 3, 2, 3, 4, 3, -1};
  vector<int> source_high = {-1, 4, 3, 4, 5, 4, -1};
  vector<int> target_low = {-1, -1, 2, 1, 4, -1, -1};
  vector<int> target_high = {-1, -1, 3, 6, 5, -1, -1};
  int source_phrase_low = 2, source_phrase_high = 5;
  int source_back_low = 2, source_back_high = 5;
  vector<int> matching = {12, 14};
  vector<int> chunklen = {1, 1};
  vector<pair<int, int>> source_gaps, target_gaps;
  int num_symbols = 3;
  EXPECT_FALSE(helper->GetGaps(source_gaps, target_gaps, matching, chunklen,
                               source_low, source_high, target_low, target_high,
                               source_phrase_low, source_phrase_high,
                               source_back_low, source_back_high, 5, 10,
                               num_symbols, met_constraints));
}

TEST_F(RuleExtractorHelperTest, TestGetSourceIndexes) {
  helper = make_shared<RuleExtractorHelper>(source_data_array,
      target_data_array, alignment, 10, 5, true, true, true);

  vector<int> matching = {13, 18, 21};
  vector<int> chunklen = {3, 2, 1};
  unordered_map<int, int> expected_indexes = {
      {3, 1}, {4, 2}, {5, 3}, {8, 5}, {9, 6}, {11, 8}
  };
  EXPECT_EQ(expected_indexes, helper->GetSourceIndexes(matching, chunklen,
                                                       1, 10));

  matching = {12, 17};
  chunklen = {2, 4};
  expected_indexes = {{2, 0}, {3, 1}, {7, 3}, {8, 4}, {9, 5}, {10, 6}};
  EXPECT_EQ(expected_indexes, helper->GetSourceIndexes(matching, chunklen,
                                                       0, 10));
}

} // namespace
} // namespace extractor
