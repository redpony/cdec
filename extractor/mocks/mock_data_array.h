#include <gmock/gmock.h>

#include "data_array.h"

namespace extractor {

class MockDataArray : public DataArray {
 public:
  MOCK_CONST_METHOD0(GetData, const vector<int>&());
  MOCK_CONST_METHOD1(AtIndex, int(int index));
  MOCK_CONST_METHOD1(GetWordAtIndex, string(int index));
  MOCK_CONST_METHOD0(GetSize, int());
  MOCK_CONST_METHOD0(GetVocabularySize, int());
  MOCK_CONST_METHOD1(HasWord, bool(const string& word));
  MOCK_CONST_METHOD1(GetWordId, int(const string& word));
  MOCK_CONST_METHOD1(GetWord, string(int word_id));
  MOCK_CONST_METHOD1(GetSentenceLength, int(int sentence_id));
  MOCK_CONST_METHOD0(GetNumSentences, int());
  MOCK_CONST_METHOD1(GetSentenceStart, int(int sentence_id));
  MOCK_CONST_METHOD1(GetSentenceId, int(int position));
};

} // namespace extractor
