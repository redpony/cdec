#ifndef _SAMPLER_H_
#define _SAMPLER_H_

#include <memory>

using namespace std;

class PhraseLocation;
class SuffixArray;

class Sampler {
 public:
  Sampler(shared_ptr<SuffixArray> suffix_array, int max_samples);

  PhraseLocation Sample(const PhraseLocation& location) const;

 private:
  int Round(double x) const;

  shared_ptr<SuffixArray> suffix_array;
  int max_samples;
};

#endif
