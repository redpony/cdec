#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <boost/filesystem.hpp>

#include "data_array.h"

using namespace std;
using namespace ::testing;
namespace fs = boost::filesystem;

namespace {

class DataArrayTest : public Test {
 protected:
  virtual void SetUp() {
    string sample_test_file("sample_bitext.txt");
    source_data = make_shared<DataArray>(sample_test_file, SOURCE);
    target_data = make_shared<DataArray>(sample_test_file, TARGET);
  }

  shared_ptr<DataArray> source_data;
  shared_ptr<DataArray> target_data;
};

TEST_F(DataArrayTest, TestGetData) {
  vector<int> expected_source_data{2, 3, 4, 5, 1, 2, 6, 7, 8, 5, 1};
  EXPECT_EQ(expected_source_data, source_data->GetData());
  EXPECT_EQ(expected_source_data.size(), source_data->GetSize());
  for (size_t i = 0; i < expected_source_data.size(); ++i) {
    EXPECT_EQ(expected_source_data[i], source_data->AtIndex(i));
  }

  vector<int> expected_target_data{2, 3, 4, 5, 1, 2, 6, 7, 8, 9, 10, 5, 1};
  EXPECT_EQ(expected_target_data, target_data->GetData());
  EXPECT_EQ(expected_target_data.size(), target_data->GetSize());
  for (size_t i = 0; i < expected_target_data.size(); ++i) {
    EXPECT_EQ(expected_target_data[i], target_data->AtIndex(i));
  }
}

TEST_F(DataArrayTest, TestVocabulary) {
  EXPECT_EQ(9, source_data->GetVocabularySize());
  EXPECT_TRUE(source_data->HasWord("mere"));
  EXPECT_EQ(4, source_data->GetWordId("mere"));
  EXPECT_EQ("mere", source_data->GetWord(4));
  EXPECT_FALSE(source_data->HasWord("banane"));

  EXPECT_EQ(11, target_data->GetVocabularySize());
  EXPECT_TRUE(target_data->HasWord("apples"));
  EXPECT_EQ(4, target_data->GetWordId("apples"));
  EXPECT_EQ("apples", target_data->GetWord(4));
  EXPECT_FALSE(target_data->HasWord("bananas"));
}

TEST_F(DataArrayTest, TestSentenceData) {
  EXPECT_EQ(2, source_data->GetNumSentences());
  EXPECT_EQ(0, source_data->GetSentenceStart(0));
  EXPECT_EQ(5, source_data->GetSentenceStart(1));
  EXPECT_EQ(11, source_data->GetSentenceStart(2));

  EXPECT_EQ(2, target_data->GetNumSentences());
  EXPECT_EQ(0, target_data->GetSentenceStart(0));
  EXPECT_EQ(5, target_data->GetSentenceStart(1));
  EXPECT_EQ(13, target_data->GetSentenceStart(2));
}

}  // namespace
