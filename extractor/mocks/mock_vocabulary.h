#include <gmock/gmock.h>

#include "../vocabulary.h"

class MockVocabulary : public Vocabulary {
 public:
  MOCK_METHOD1(GetTerminalValue, string(int word_id));
  MOCK_METHOD1(GetTerminalIndex, int(const string& word));
};
