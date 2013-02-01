#ifndef _FEATURE_H_
#define _FEATURE_H_

#include <string>

//TODO(pauldb): include headers nicely.
#include "../phrase.h"

using namespace std;

struct FeatureContext {
  FeatureContext(const Phrase& source_phrase, const Phrase& target_phrase,
                 double sample_source_count, int pair_count) :
    source_phrase(source_phrase), target_phrase(target_phrase),
    sample_source_count(sample_source_count), pair_count(pair_count) {}

  Phrase source_phrase;
  Phrase target_phrase;
  double sample_source_count;
  int pair_count;
};

class Feature {
 public:
  virtual double Score(const FeatureContext& context) const = 0;

  virtual string GetName() const = 0;

  static const double MAX_SCORE;
};

#endif
