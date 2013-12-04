#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "mocks/mock_alignment.h"
#include "mocks/mock_data_array.h"
#include "translation_table.h"

using namespace std;
using namespace ::testing;
namespace ar = boost::archive;

namespace extractor {
namespace {

class TranslationTableTest : public Test {
 protected:
  virtual void SetUp() {
    vector<string> words = {"a", "b", "c"};

    vector<int> source_data = {2, 3, 2, 3, 4, 0, 2, 3, 6, 0, 2, 3, 6, 0};
    vector<int> source_sentence_start = {0, 6, 10, 14};
    shared_ptr<MockDataArray> source_data_array = make_shared<MockDataArray>();
    EXPECT_CALL(*source_data_array, GetData())
        .WillRepeatedly(Return(source_data));
    EXPECT_CALL(*source_data_array, GetNumSentences())
        .WillRepeatedly(Return(3));
    for (size_t i = 0; i < source_sentence_start.size(); ++i) {
      EXPECT_CALL(*source_data_array, GetSentenceStart(i))
          .WillRepeatedly(Return(source_sentence_start[i]));
    }
    for (size_t i = 0; i < words.size(); ++i) {
      EXPECT_CALL(*source_data_array, GetWordId(words[i]))
          .WillRepeatedly(Return(i + 2));
    }
    EXPECT_CALL(*source_data_array, GetWordId("d")).WillRepeatedly(Return(-1));

    vector<int> target_data = {2, 3, 2, 3, 4, 5, 0, 3, 6, 0, 2, 7, 0};
    vector<int> target_sentence_start = {0, 7, 10, 13};
    shared_ptr<MockDataArray> target_data_array = make_shared<MockDataArray>();
    EXPECT_CALL(*target_data_array, GetData())
        .WillRepeatedly(Return(target_data));
    for (size_t i = 0; i < target_sentence_start.size(); ++i) {
      EXPECT_CALL(*target_data_array, GetSentenceStart(i))
          .WillRepeatedly(Return(target_sentence_start[i]));
    }
    for (size_t i = 0; i < words.size(); ++i) {
      EXPECT_CALL(*target_data_array, GetWordId(words[i]))
          .WillRepeatedly(Return(i + 2));
    }
    EXPECT_CALL(*target_data_array, GetWordId("d")).WillRepeatedly(Return(-1));

    vector<pair<int, int>> links1 = {
      make_pair(0, 0), make_pair(1, 1), make_pair(2, 2), make_pair(3, 3),
      make_pair(4, 4), make_pair(4, 5)
    };
    vector<pair<int, int>> links2 = {make_pair(1, 0), make_pair(2, 1)};
    vector<pair<int, int>> links3 = {make_pair(0, 0), make_pair(2, 1)};
    shared_ptr<MockAlignment> alignment = make_shared<MockAlignment>();
    EXPECT_CALL(*alignment, GetLinks(0)).WillRepeatedly(Return(links1));
    EXPECT_CALL(*alignment, GetLinks(1)).WillRepeatedly(Return(links2));
    EXPECT_CALL(*alignment, GetLinks(2)).WillRepeatedly(Return(links3));

    table = TranslationTable(source_data_array, target_data_array, alignment);
  }

  TranslationTable table;
};

TEST_F(TranslationTableTest, TestScores) {
  EXPECT_EQ(0.75, table.GetTargetGivenSourceScore("a", "a"));
  EXPECT_EQ(0, table.GetTargetGivenSourceScore("a", "b"));
  EXPECT_EQ(0.5, table.GetTargetGivenSourceScore("c", "c"));
  EXPECT_EQ(-1, table.GetTargetGivenSourceScore("c", "d"));

  EXPECT_EQ(1, table.GetSourceGivenTargetScore("a", "a"));
  EXPECT_EQ(0, table.GetSourceGivenTargetScore("a", "b"));
  EXPECT_EQ(1, table.GetSourceGivenTargetScore("c", "c"));
  EXPECT_EQ(-1, table.GetSourceGivenTargetScore("c", "d"));
}

TEST_F(TranslationTableTest, TestSerialization) {
  stringstream stream(ios_base::binary | ios_base::out | ios_base::in);
  ar::binary_oarchive output_stream(stream, ar::no_header);
  output_stream << table;

  TranslationTable table_copy;
  ar::binary_iarchive input_stream(stream, ar::no_header);
  input_stream >> table_copy;

  EXPECT_EQ(table, table_copy);
}

} // namespace
} // namespace extractor
