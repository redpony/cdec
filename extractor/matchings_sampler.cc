#include "matchings_sampler.h"

#include "data_array.h"
#include "phrase_location.h"

namespace extractor {

MatchingsSampler::MatchingsSampler(
    shared_ptr<DataArray> data_array, int max_samples) :
    BackoffSampler(data_array, max_samples) {}

MatchingsSampler::MatchingsSampler() {}

int MatchingsSampler::GetNumSubpatterns(const PhraseLocation& location) const {
  return location.num_subpatterns;
}

int MatchingsSampler::GetRangeLow(const PhraseLocation&) const {
  return 0;
}

int MatchingsSampler::GetRangeHigh(const PhraseLocation& location) const {
  return location.matchings->size() / location.num_subpatterns;
}

int MatchingsSampler::GetPosition(const PhraseLocation& location,
                                  int index) const {
  return (*location.matchings)[index * location.num_subpatterns];
}

void MatchingsSampler::AppendMatching(vector<int>& samples, int index,
                                      const PhraseLocation& location) const {
  int start = index * location.num_subpatterns;
  copy(location.matchings->begin() + start,
       location.matchings->begin() + start + location.num_subpatterns,
       back_inserter(samples));
}

} // namespace extractor
