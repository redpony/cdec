#ifndef _PHRASE_LOCATION_SAMPLER_H_
#define _PHRASE_LOCATION_SAMPLER_H_

#include <memory>

#include "sampler.h"

namespace extractor {

class MatchingsSampler;
class PhraseLocation;
class SuffixArray;
class SuffixArrayRangeSampler;

class PhraseLocationSampler : public Sampler {
 public:
  PhraseLocationSampler(shared_ptr<SuffixArray> suffix_array, int max_samples);

  // For testing only.
  PhraseLocationSampler(
      shared_ptr<MatchingsSampler> matchings_sampler,
      shared_ptr<SuffixArrayRangeSampler> suffix_array_sampler);

  PhraseLocation Sample(
      const PhraseLocation& location,
      const unordered_set<int>& blacklisted_sentence_ids) const;

 private:
  shared_ptr<MatchingsSampler> matchings_sampler;
  shared_ptr<SuffixArrayRangeSampler> suffix_array_sampler;
};

} // namespace extractor

#endif
