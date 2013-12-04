#ifndef _SUFFIX_ARRAY_SAMPLER_H_
#define _SUFFIX_ARRAY_SAMPLER_H_

#include "backoff_sampler.h"

namespace extractor {

class SuffixArray;

class SuffixArrayRangeSampler : public BackoffSampler {
 public:
  SuffixArrayRangeSampler(shared_ptr<SuffixArray> suffix_array,
                          int max_samples);

  SuffixArrayRangeSampler();

 private:
  int GetNumSubpatterns(const PhraseLocation& location) const;

  int GetRangeLow(const PhraseLocation& location) const;

  int GetRangeHigh(const PhraseLocation& location) const;

  int GetPosition(const PhraseLocation& location, int index) const;

  void AppendMatching(vector<int>& samples, int index,
                      const PhraseLocation& location) const;

  shared_ptr<SuffixArray> source_suffix_array;
};

} // namespace extractor

#endif
