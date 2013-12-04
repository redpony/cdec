#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "vocabulary.h"

using namespace std;
using namespace ::testing;
namespace ar = boost::archive;

namespace extractor {
namespace {

TEST(VocabularyTest, TestIndexes) {
  Vocabulary vocabulary;
  EXPECT_EQ(0, vocabulary.GetTerminalIndex("zero"));
  EXPECT_EQ("zero", vocabulary.GetTerminalValue(0));

  EXPECT_EQ(1, vocabulary.GetTerminalIndex("one"));
  EXPECT_EQ("one", vocabulary.GetTerminalValue(1));
}

TEST(VocabularyTest, TestSerialization) {
  Vocabulary vocabulary;
  EXPECT_EQ(0, vocabulary.GetTerminalIndex("zero"));
  EXPECT_EQ("zero", vocabulary.GetTerminalValue(0));

  stringstream stream(ios_base::out | ios_base::in);
  ar::text_oarchive output_stream(stream, ar::no_header);
  output_stream << vocabulary;

  Vocabulary vocabulary_copy;
  ar::text_iarchive input_stream(stream, ar::no_header);
  input_stream >> vocabulary_copy;

  EXPECT_EQ(vocabulary, vocabulary_copy);
}

} // namespace
} // namespace extractor
