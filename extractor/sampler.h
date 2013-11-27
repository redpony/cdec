#ifndef _SAMPLER_H_
#define _SAMPLER_H_

#include <memory>
#include <unordered_set>

using namespace std;

namespace extractor {

class PhraseLocation;

/**
 * Base sampler class.
 */
class Sampler {
 public:
  virtual PhraseLocation Sample(
      const PhraseLocation& location,
      const unordered_set<int>& blacklisted_sentence_ids) const = 0;
};

} // namespace extractor

#endif
