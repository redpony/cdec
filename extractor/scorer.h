#ifndef _SCORER_H_
#define _SCORER_H_

#include <vector>

#include "features/feature.h"

using namespace std;

class Scorer {
 public:
  Scorer(const vector<Feature*>& features);
  ~Scorer();

 private:
  vector<Feature*> features;
};

#endif
