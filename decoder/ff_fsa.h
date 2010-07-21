#ifndef FF_FSA_H
#define FF_FSA_H

#include <stdint.h> //C99
#include <string>
#include "ff.h"
#include "sparse_vector.h"
#include "value_array.h"
#include "tdict.h"

typedef ValueArray<uint8_t> Bytes;

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
struct FeatureFunctionFromFsa : public FeatureFunction {
  Impl& d() { return static_cast<Impl&>(*this); }
  Impl const& d() { return static_cast<Impl const&>(*this); }

  FeatureFunctionFromFsa() {  }
  Init() {
    name=d().name;
    SetStateSize(sizeof(WordID)*2*MarkovOrder
  } // can't do this in constructor because we come before d() in order

  virtual Features Features() const { return d().Features(); }

};


#endif
