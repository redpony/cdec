#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "mocks/mock_vocabulary.h"
#include "phrase.h"
#include "phrase_builder.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class PhraseTest : public Test {
 protected:
  virtual void SetUp() {
    shared_ptr<MockVocabulary> vocabulary = make_shared<MockVocabulary>();
    vector<string> words = {"w1", "w2", "w3", "w4"};
    for (size_t i = 0; i < words.size(); ++i) {
      EXPECT_CALL(*vocabulary, GetTerminalValue(i + 1))
          .WillRepeatedly(Return(words[i]));
    }
    shared_ptr<PhraseBuilder> phrase_builder =
        make_shared<PhraseBuilder>(vocabulary);

    symbols1 = vector<int>{1, 2, 3};
    phrase1 = phrase_builder->Build(symbols1);
    symbols2 = vector<int>{1, 2, -1, 3, -2, 4};
    phrase2 = phrase_builder->Build(symbols2);
  }

  vector<int> symbols1, symbols2;
  Phrase phrase1, phrase2;
};

TEST_F(PhraseTest, TestArity) {
  EXPECT_EQ(0, phrase1.Arity());
  EXPECT_EQ(2, phrase2.Arity());
}

TEST_F(PhraseTest, GetChunkLen) {
  EXPECT_EQ(3, phrase1.GetChunkLen(0));

  EXPECT_EQ(2, phrase2.GetChunkLen(0));
  EXPECT_EQ(1, phrase2.GetChunkLen(1));
  EXPECT_EQ(1, phrase2.GetChunkLen(2));
}

TEST_F(PhraseTest, TestGet) {
  EXPECT_EQ(symbols1, phrase1.Get());
  EXPECT_EQ(symbols2, phrase2.Get());
}

TEST_F(PhraseTest, TestGetSymbol) {
  for (size_t i = 0; i < symbols1.size(); ++i) {
    EXPECT_EQ(symbols1[i], phrase1.GetSymbol(i));
  }
  for (size_t i = 0; i < symbols2.size(); ++i) {
    EXPECT_EQ(symbols2[i], phrase2.GetSymbol(i));
  }
}

TEST_F(PhraseTest, TestGetNumSymbols) {
  EXPECT_EQ(3, phrase1.GetNumSymbols());
  EXPECT_EQ(6, phrase2.GetNumSymbols());
}

TEST_F(PhraseTest, TestGetWords) {
  vector<string> expected_words = {"w1", "w2", "w3"};
  EXPECT_EQ(expected_words, phrase1.GetWords());
  expected_words = {"w1", "w2", "w3", "w4"};
  EXPECT_EQ(expected_words, phrase2.GetWords());
}

TEST_F(PhraseTest, TestComparator) {
  EXPECT_FALSE(phrase1 < phrase2);
  EXPECT_TRUE(phrase2 < phrase1);
}

} // namespace
} // namespace extractor
