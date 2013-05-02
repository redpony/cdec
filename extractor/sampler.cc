#include "sampler.h"

#include "phrase_location.h"
#include "suffix_array.h"

namespace extractor {

Sampler::Sampler(shared_ptr<SuffixArray> suffix_array, int max_samples) :
    suffix_array(suffix_array), max_samples(max_samples) {}

Sampler::Sampler() {}

Sampler::~Sampler() {}

PhraseLocation Sampler::Sample(const PhraseLocation& location) const {
  vector<int> sample;
  int num_subpatterns;
  if (location.matchings == NULL) {
    // Sample suffix array range.
    num_subpatterns = 1;
    int low = location.sa_low, high = location.sa_high;
    double step = max(1.0, (double) (high - low) / max_samples);
    for (double i = low; i < high && sample.size() < max_samples; i += step) {
      sample.push_back(suffix_array->GetSuffix(Round(i)));
    }
  } else {
    // Sample vector of occurrences.
    num_subpatterns = location.num_subpatterns;
    int num_matchings = location.matchings->size() / num_subpatterns;
    double step = max(1.0, (double) num_matchings / max_samples);
    for (double i = 0, num_samples = 0;
         i < num_matchings && num_samples < max_samples;
         i += step, ++num_samples) {
      int start = Round(i) * num_subpatterns;
      sample.insert(sample.end(), location.matchings->begin() + start,
          location.matchings->begin() + start + num_subpatterns);
    }
  }
  return PhraseLocation(sample, num_subpatterns);
}

int Sampler::Round(double x) const {
  return x + 0.5;
}

} // namespace extractor
