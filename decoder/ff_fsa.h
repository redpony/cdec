#ifndef FF_FSA_H
#define FF_FSA_H

#include <string>
#include "ff.h"
#include "sparse_vector.h"
/*

 */
struct FsaFeatureFunction {
  std::string name;

  // state for backoff

  // scan

  // heuristic

  // all strings x of this length must end in the same state
  virtual int MarkovOrder() const {
    return 0;
  }


};


// regular bottom up scorer from Fsa feature
template <class Impl>
struct FeatureFunctionFromFsa {
};


#endif
