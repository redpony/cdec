#ifndef _BACKOFF_SAMPLER_H_
#define _BACKOFF_SAMPLER_H_

#include <vector>

#include "sampler.h"

namespace extractor {

class DataArray;
class PhraseLocation;

class BackoffSampler : public Sampler {
 public:
  BackoffSampler(shared_ptr<DataArray> source_data_array, int max_samples);

  BackoffSampler();

  PhraseLocation Sample(
      const PhraseLocation& location,
      const unordered_set<int>& blacklisted_sentence_ids) const;

 private:
  virtual int GetNumSubpatterns(const PhraseLocation& location) const = 0;

  virtual int GetRangeLow(const PhraseLocation& location) const = 0;

  virtual int GetRangeHigh(const PhraseLocation& location) const = 0;

  virtual int GetPosition(const PhraseLocation& location, int index) const = 0;

  virtual void AppendMatching(vector<int>& samples, int index,
                              const PhraseLocation& location) const = 0;

  shared_ptr<DataArray> source_data_array;
  int max_samples;
};

} // namespace extractor

#endif
