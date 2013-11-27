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
  int last_position = low - 1;
  double step = max(1.0, (double) (high - low) / max_samples);
  for (double num_samples = 0, i = low;
       num_samples < max_samples && i < high;
       ++num_samples, i += step) {
    int position = GetPosition(location, round(i));
    int sentence_id = source_data_array->GetSentenceId(position);
    bool found = false;
    if (last_position >= position ||
        blacklisted_sentence_ids.count(sentence_id)) {
      for (double backoff_step = 1; backoff_step < step; ++backoff_step) {
        double j = i - backoff_step;
        if (round(j) >= 0) {
          position = GetPosition(location, round(j));
          sentence_id = source_data_array->GetSentenceId(position);
          if (position > last_position &&
              !blacklisted_sentence_ids.count(sentence_id)) {
            found = true;
            last_position = position;
            break;
          }
        }

        double k = i + backoff_step;
        if (round(k) < high) {
          position = GetPosition(location, round(k));
          sentence_id = source_data_array->GetSentenceId(position);
          if (!blacklisted_sentence_ids.count(sentence_id)) {
            found = true;
            last_position = position;
            break;
          }
        }
      }
    } else {
      found = true;
      last_position = position;
    }

    if (found) {
      AppendMatching(samples, position, location);
    }
  }

  return PhraseLocation(samples, GetNumSubpatterns(location));
}

} // namespace extractor
