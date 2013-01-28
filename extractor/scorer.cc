#include "scorer.h"

Scorer::Scorer(const vector<Feature*>& features) : features(features) {}

Scorer::~Scorer() {
  for (Feature* feature: features) {
    delete feature;
  }
}
