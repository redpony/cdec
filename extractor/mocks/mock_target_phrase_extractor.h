#include <gmock/gmock.h>

#include "target_phrase_extractor.h"

namespace extractor {

typedef pair<Phrase, PhraseAlignment> PhraseExtract;

class MockTargetPhraseExtractor : public TargetPhraseExtractor {
 public:
  MOCK_CONST_METHOD6(ExtractPhrases, vector<PhraseExtract>(
      const vector<pair<int, int> > &, const vector<int>&, int, int,
      const unordered_map<int, int>&, int));
};

} // namespace extractor
