#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/filesystem.hpp>

#include "data_array.h"

using namespace std;
using namespace ::testing;
namespace fs = boost::filesystem;
namespace ar = boost::archive;

namespace extractor {
namespace {

class DataArrayTest : public Test {
 protected:
  virtual void SetUp() {
    string sample_test_file("sample_bitext.txt");
    source_data = DataArray(sample_test_file, SOURCE);
    target_data = DataArray(sample_test_file, TARGET);
  }

  DataArray source_data;
  DataArray target_data;
};

TEST_F(DataArrayTest, TestGetData) {
  vector<int> expected_source_data = {2, 3, 4, 5, 1, 2, 6, 7, 8, 5, 1};
  vector<string> expected_source_words = {
      "ana", "are", "mere", ".", "__END_OF_LINE__",
      "ana", "bea", "mult", "lapte", ".", "__END_OF_LINE__"
  };
  EXPECT_EQ(expected_source_data, source_data.GetData());
  EXPECT_EQ(expected_source_data.size(), source_data.GetSize());
  for (size_t i = 0; i < expected_source_data.size(); ++i) {
    EXPECT_EQ(expected_source_data[i], source_data.AtIndex(i));
    EXPECT_EQ(expected_source_words[i], source_data.GetWordAtIndex(i));
  }

  vector<int> expected_target_data = {2, 3, 4, 5, 1, 2, 6, 7, 8, 9, 10, 5, 1};
  vector<string> expected_target_words = {
      "anna", "has", "apples", ".", "__END_OF_LINE__",
      "anna", "drinks", "a", "lot", "of", "milk", ".", "__END_OF_LINE__"
  };
  EXPECT_EQ(expected_target_data, target_data.GetData());
  EXPECT_EQ(expected_target_data.size(), target_data.GetSize());
  for (size_t i = 0; i < expected_target_data.size(); ++i) {
    EXPECT_EQ(expected_target_data[i], target_data.AtIndex(i));
    EXPECT_EQ(expected_target_words[i], target_data.GetWordAtIndex(i));
  }
}

TEST_F(DataArrayTest, TestSubstrings) {
  vector<int> expected_word_ids = {3, 4, 5};
  vector<string> expected_words = {"are", "mere", "."};
  EXPECT_EQ(expected_word_ids, source_data.GetWordIds(1, 3));
  EXPECT_EQ(expected_words, source_data.GetWords(1, 3));

  expected_word_ids = {7, 8};
  expected_words = {"a", "lot"};
  EXPECT_EQ(expected_word_ids, target_data.GetWordIds(7, 2));
  EXPECT_EQ(expected_words, target_data.GetWords(7, 2));
}

TEST_F(DataArrayTest, TestVocabulary) {
  EXPECT_EQ(9, source_data.GetVocabularySize());
  EXPECT_EQ(4, source_data.GetWordId("mere"));
  EXPECT_EQ("mere", source_data.GetWord(4));

  EXPECT_EQ(11, target_data.GetVocabularySize());
  EXPECT_EQ(4, target_data.GetWordId("apples"));
  EXPECT_EQ("apples", target_data.GetWord(4));
}

TEST_F(DataArrayTest, TestSentenceData) {
  EXPECT_EQ(2, source_data.GetNumSentences());
  EXPECT_EQ(0, source_data.GetSentenceStart(0));
  EXPECT_EQ(5, source_data.GetSentenceStart(1));
  EXPECT_EQ(11, source_data.GetSentenceStart(2));

  EXPECT_EQ(4, source_data.GetSentenceLength(0));
  EXPECT_EQ(5, source_data.GetSentenceLength(1));

  vector<int> expected_source_ids = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1};
  for (size_t i = 0; i < expected_source_ids.size(); ++i) {
    EXPECT_EQ(expected_source_ids[i], source_data.GetSentenceId(i));
  }

  EXPECT_EQ(2, target_data.GetNumSentences());
  EXPECT_EQ(0, target_data.GetSentenceStart(0));
  EXPECT_EQ(5, target_data.GetSentenceStart(1));
  EXPECT_EQ(13, target_data.GetSentenceStart(2));

  EXPECT_EQ(4, target_data.GetSentenceLength(0));
  EXPECT_EQ(7, target_data.GetSentenceLength(1));

  vector<int> expected_target_ids = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1};
  for (size_t i = 0; i < expected_target_ids.size(); ++i) {
    EXPECT_EQ(expected_target_ids[i], target_data.GetSentenceId(i));
  }
}

TEST_F(DataArrayTest, TestSerialization) {
  stringstream stream(ios_base::binary | ios_base::out | ios_base::in);
  ar::binary_oarchive output_stream(stream, ar::no_header);
  output_stream << source_data << target_data;

  DataArray source_copy, target_copy;
  ar::binary_iarchive input_stream(stream, ar::no_header);
  input_stream >> source_copy >> target_copy;

  EXPECT_EQ(source_data, source_copy);
  EXPECT_EQ(target_data, target_copy);
}

} // namespace
} // namespace extractor
