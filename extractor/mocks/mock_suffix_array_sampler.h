#include <gmock/gmock.h>

#include "phrase_location.h"
#include "suffix_array_sampler.h"

namespace extractor {

class MockSuffixArraySampler : public SuffixArrayRangeSampler {
 public:
  MOCK_CONST_METHOD2(Sample, PhraseLocation(
      const PhraseLocation& location,
      const unordered_set<int>& blacklisted_sentence_ids));
};

} // namespace extractor
