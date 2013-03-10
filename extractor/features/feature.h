#ifndef _FEATURE_H_
#define _FEATURE_H_

#include <string>

#include "phrase.h"

using namespace std;

namespace extractor {
namespace features {

/**
 * Structure providing context for computing feature scores.
 */
struct FeatureContext {
  FeatureContext(const Phrase& source_phrase, const Phrase& target_phrase,
                 double source_phrase_count, int pair_count, int num_samples) :
    source_phrase(source_phrase), target_phrase(target_phrase),
    source_phrase_count(source_phrase_count), pair_count(pair_count),
    num_samples(num_samples) {}

  Phrase source_phrase;
  Phrase target_phrase;
  double source_phrase_count;
  int pair_count;
  int num_samples;
};

/**
 * Base class for features.
 */
class Feature {
 public:
  virtual double Score(const FeatureContext& context) const = 0;

  virtual string GetName() const = 0;

  virtual ~Feature();

  static const double MAX_SCORE;
};

} // namespace features
} // namespace extractor

#endif
