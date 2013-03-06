#ifndef _SCORER_H_
#define _SCORER_H_

#include <memory>
#include <string>
#include <vector>

using namespace std;

namespace extractor {

namespace features {
  class Feature;
  class FeatureContext;
} // namespace features

class Scorer {
 public:
  Scorer(const vector<shared_ptr<features::Feature> >& features);

  virtual ~Scorer();

  virtual vector<double> Score(const features::FeatureContext& context) const;

  virtual vector<string> GetFeatureNames() const;

 protected:
  Scorer();

 private:
  vector<shared_ptr<features::Feature> > features;
};

} // namespace extractor

#endif
