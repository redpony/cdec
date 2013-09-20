#ifndef _SAMPLER_H_
#define _SAMPLER_H_

#include <memory>
#include <unordered_set>

#include "data_array.h"

using namespace std;

namespace extractor {

class PhraseLocation;
class SuffixArray;

/**
 * Provides uniform sampling for a PhraseLocation.
 */
class Sampler {
 public:
  Sampler(shared_ptr<SuffixArray> suffix_array, int max_samples);

  virtual ~Sampler();

  // Samples uniformly at most max_samples phrase occurrences.
  virtual PhraseLocation Sample(const PhraseLocation& location, const unordered_set<int> blacklisted_sentence_ids, const shared_ptr<DataArray> source_data_array) const;

 protected:
  Sampler();

 private:
  // Round floating point number to the nearest integer.
  int Round(double x) const;

  shared_ptr<SuffixArray> suffix_array;
  int max_samples;
};

} // namespace extractor

#endif
