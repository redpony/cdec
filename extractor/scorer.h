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

/**
 * Computes the feature scores for a source-target phrase pair.
 */
class Scorer {
 public:
  Scorer(const vector<shared_ptr<features::Feature> >& features);

  virtual ~Scorer();

  // Computes the feature score for the given context.
  virtual vector<double> Score(const features::FeatureContext& context) const;

  // Returns the set of feature names used to score any context.
  virtual vector<string> GetFeatureNames() const;

 protected:
  Scorer();

 private:
  vector<shared_ptr<features::Feature> > features;
};

} // namespace extractor

#endif
