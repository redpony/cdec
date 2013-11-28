#include "backoff_sampler.h"

#include "data_array.h"
#include "phrase_location.h"

namespace extractor {

BackoffSampler::BackoffSampler(
    shared_ptr<DataArray> source_data_array, int max_samples) :
    source_data_array(source_data_array), max_samples(max_samples) {}

BackoffSampler::BackoffSampler() {}

PhraseLocation BackoffSampler::Sample(
    const PhraseLocation& location,
    const unordered_set<int>& blacklisted_sentence_ids) const {
  vector<int> samples;
  int low = GetRangeLow(location), high = GetRangeHigh(location);
  int last = low - 1;
  double step = max(1.0, (double) (high - low) / max_samples);
  for (double num_samples = 0, i = low;
       num_samples < max_samples && i < high;
       ++num_samples, i += step) {
    int sample = round(i);
    int position = GetPosition(location, sample);
    int sentence_id = source_data_array->GetSentenceId(position);
    bool found = false;
    if (last >= sample ||
        blacklisted_sentence_ids.count(sentence_id)) {
      for (double backoff_step = 1; backoff_step < step; ++backoff_step) {
        double j = i - backoff_step;
        sample = round(j);
        if (sample >= 0) {
          position = GetPosition(location, sample);
          sentence_id = source_data_array->GetSentenceId(position);
          if (sample > last && !blacklisted_sentence_ids.count(sentence_id)) {
            found = true;
            break;
          }
        }

        double k = i + backoff_step;
        sample = round(k);
        if (sample < high) {
          position = GetPosition(location, sample);
          sentence_id = source_data_array->GetSentenceId(position);
          if (!blacklisted_sentence_ids.count(sentence_id)) {
            found = true;
            break;
          }
        }
      }
    } else {
      found = true;
    }

    if (found) {
      last = sample;
      AppendMatching(samples, sample, location);
    }
  }

  return PhraseLocation(samples, GetNumSubpatterns(location));
}

} // namespace extractor
