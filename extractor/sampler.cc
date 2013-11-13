#include "sampler.h"

#include "phrase_location.h"
#include "suffix_array.h"

namespace extractor {

Sampler::Sampler(shared_ptr<SuffixArray> suffix_array, int max_samples) :
    suffix_array(suffix_array), max_samples(max_samples) {}

Sampler::Sampler() {}

Sampler::~Sampler() {}

PhraseLocation Sampler::Sample(const PhraseLocation& location, const unordered_set<int>& blacklisted_sentence_ids, const shared_ptr<DataArray> source_data_array) const {
  vector<int> sample;
  int num_subpatterns;
  if (location.matchings == NULL) {
    // Sample suffix array range.
    num_subpatterns = 1;
    int low = location.sa_low, high = location.sa_high;
    double step = max(1.0, (double) (high - low) / max_samples);
    double i = low, last = i;
    bool found;
    while (sample.size() < max_samples && i < high) {
      int x = suffix_array->GetSuffix(Round(i));
      int id = source_data_array->GetSentenceId(x);
      if (find(blacklisted_sentence_ids.begin(), blacklisted_sentence_ids.end(), id) != blacklisted_sentence_ids.end()) {
        found = false;
        double backoff_step = 1;
        while (true) {
          if ((double)backoff_step >= step) break;
          double j = i - backoff_step;
          x = suffix_array->GetSuffix(Round(j));
          id = source_data_array->GetSentenceId(x);
          if (x >= 0 && j > last && find(blacklisted_sentence_ids.begin(), blacklisted_sentence_ids.end(), id) == blacklisted_sentence_ids.end()) {
            found = true; last = i; break;
          }
          double k = i + backoff_step;
          x = suffix_array->GetSuffix(Round(k));
          id = source_data_array->GetSentenceId(x);
          if (k < min(i+step, (double)high) && find(blacklisted_sentence_ids.begin(), blacklisted_sentence_ids.end(), id) == blacklisted_sentence_ids.end()) {
            found = true; last = k; break;
          }
          if (j <= last && k >= high) break;
          backoff_step++;
        }
      } else {
        found = true;
        last = i;
      }
      if (found) sample.push_back(x);
      i += step;
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
