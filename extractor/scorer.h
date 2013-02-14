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

  virtual ~Scorer();

  virtual vector<double> Score(const FeatureContext& context) const;

  virtual vector<string> GetFeatureNames() const;

 protected:
  Scorer();

 private:
  vector<shared_ptr<Feature> > features;
};

#endif
