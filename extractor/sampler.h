#ifndef _SAMPLER_H_
#define _SAMPLER_H_

#include <memory>

using namespace std;

namespace extractor {

class PhraseLocation;
class SuffixArray;

class Sampler {
 public:
  Sampler(shared_ptr<SuffixArray> suffix_array, int max_samples);

  virtual ~Sampler();

  virtual PhraseLocation Sample(const PhraseLocation& location) const;

 protected:
  Sampler();

 private:
  int Round(double x) const;

  shared_ptr<SuffixArray> suffix_array;
  int max_samples;
};

} // namespace extractor

#endif
