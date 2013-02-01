#include "target_given_source_coherent.h"

#include <cmath>

double TargetGivenSourceCoherent::Score(const FeatureContext& context) const {
  double prob = context.pair_count / context.sample_source_count;
  return prob > 0 ? -log10(prob) : MAX_SCORE;
}

string TargetGivenSourceCoherent::GetName() const {
  return "EGivenFCoherent";
}
