#ifndef _MATCHINGS_SAMPLER_H_
#define _MATCHINGS_SAMPLER_H_

#include "backoff_sampler.h"

namespace extractor {

class DataArray;

class MatchingsSampler : public BackoffSampler {
 public:
  MatchingsSampler(shared_ptr<DataArray> data_array, int max_samples);

  MatchingsSampler();

 private:
   int GetNumSubpatterns(const PhraseLocation& location) const;

   int GetRangeLow(const PhraseLocation& location) const;

   int GetRangeHigh(const PhraseLocation& location) const;

   int GetPosition(const PhraseLocation& location, int index) const;

   void AppendMatching(vector<int>& samples, int index,
                       const PhraseLocation& location) const;
};

} // namespace extractor

#endif
