#ifndef _SCORER_H_
#define _SCORER_H_

#include <memory>
#include <string>
#include <vector>

using namespace std;

class Feature;
class FeatureContext;

class Scorer {
 public:
  Scorer(const vector<shared_ptr<Feature> >& features);

  vector<double> Score(const FeatureContext& context) const;

  vector<string> GetFeatureNames() const;

 private:
  vector<shared_ptr<Feature> > features;
};

#endif
