#include "suffix_array_sampler.h"

#include "data_array.h"
#include "phrase_location.h"
#include "suffix_array.h"

namespace extractor {

SuffixArrayRangeSampler::SuffixArrayRangeSampler(
    shared_ptr<SuffixArray> source_suffix_array, int max_samples) :
    BackoffSampler(source_suffix_array->GetData(), max_samples),
    source_suffix_array(source_suffix_array) {}

SuffixArrayRangeSampler::SuffixArrayRangeSampler() {}

int SuffixArrayRangeSampler::GetNumSubpatterns(const PhraseLocation&) const {
  return 1;
}

int SuffixArrayRangeSampler::GetRangeLow(
    const PhraseLocation& location) const {
  return location.sa_low;
}

int SuffixArrayRangeSampler::GetRangeHigh(
    const PhraseLocation& location) const {
  return location.sa_high;
}

int SuffixArrayRangeSampler::GetPosition(
    const PhraseLocation&, int position) const {
  return source_suffix_array->GetSuffix(position);
}

void SuffixArrayRangeSampler::AppendMatching(
    vector<int>& samples, int index, const PhraseLocation&) const {
  samples.push_back(source_suffix_array->GetSuffix(index));
}

} // namespace extractor
