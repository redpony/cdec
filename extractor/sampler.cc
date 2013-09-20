#include "sampler.h"

#include "phrase_location.h"
#include "suffix_array.h"

namespace extractor {

Sampler::Sampler(shared_ptr<SuffixArray> suffix_array, int max_samples) :
    suffix_array(suffix_array), max_samples(max_samples) {}

Sampler::Sampler() {}

Sampler::~Sampler() {}

PhraseLocation Sampler::Sample(const PhraseLocation& location, unordered_set<int> blacklisted_sentence_ids, const shared_ptr<DataArray> source_data_array) const {
  vector<int> sample;
  int num_subpatterns;
  if (location.matchings == NULL) {
    // Sample suffix array range.
    num_subpatterns = 1;
    int low = location.sa_low, high = location.sa_high;
    double step = Round(max(1.0, (double) (high - low) / max_samples));
    int i = location.sa_low;
    bool found = false;
    while (sample.size() < max_samples && i <= location.sa_high) {
      int x = suffix_array->GetSuffix(i);
      int id = source_data_array->GetSentenceId(x);
      if (find(blacklisted_sentence_ids.begin(), blacklisted_sentence_ids.end(), id) != blacklisted_sentence_ids.end()) {
        int backoff_step = 1;
        while (true) {
          int j = i - backoff_step;
          x = suffix_array->GetSuffix(j);
          id = source_data_array->GetSentenceId(x);
          if ((j >= location.sa_low) && (find(blacklisted_sentence_ids.begin(), blacklisted_sentence_ids.end(), id) == blacklisted_sentence_ids.end())
              && (find(sample.begin(), sample.end(), x) == sample.end())) { found = true; break; }
          int k = i + backoff_step;
          x = suffix_array->GetSuffix(k);
          id = source_data_array->GetSentenceId(x);
          if ((k <= location.sa_high) && (find(blacklisted_sentence_ids.begin(), blacklisted_sentence_ids.end(), id) == blacklisted_sentence_ids.end())
              && (find(sample.begin(), sample.end(), x) == sample.end())) { found = true; break; }
          if (j <= location.sa_low && k >= location.sa_high) break;
          backoff_step++;
        }
      } else {
        found = true;
      }
      if (found && (find(sample.begin(), sample.end(), x) == sample.end())) sample.push_back(x);
      i += step;
      found = false;
    }
  } else { // when do we get here?
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
