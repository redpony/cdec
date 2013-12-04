#include "phrase_location_sampler.h"

#include "matchings_sampler.h"
#include "phrase_location.h"
#include "suffix_array.h"
#include "suffix_array_sampler.h"

namespace extractor {

PhraseLocationSampler::PhraseLocationSampler(
    shared_ptr<SuffixArray> suffix_array, int max_samples) {
  matchings_sampler = make_shared<MatchingsSampler>(
      suffix_array->GetData(), max_samples);
  suffix_array_sampler = make_shared<SuffixArrayRangeSampler>(
      suffix_array, max_samples);
}

PhraseLocationSampler::PhraseLocationSampler(
    shared_ptr<MatchingsSampler> matchings_sampler,
    shared_ptr<SuffixArrayRangeSampler> suffix_array_sampler) :
    matchings_sampler(matchings_sampler),
    suffix_array_sampler(suffix_array_sampler) {}

PhraseLocation PhraseLocationSampler::Sample(
    const PhraseLocation& location,
    const unordered_set<int>& blacklisted_sentence_ids) const {
  if (location.matchings == NULL) {
    return suffix_array_sampler->Sample(location, blacklisted_sentence_ids);
  } else {
    return matchings_sampler->Sample(location, blacklisted_sentence_ids);
  }
}

} // namespace extractor
