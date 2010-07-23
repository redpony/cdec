#ifndef FF_FSA_DYNAMIC_H
#define FF_FSA_DYNAMIC_H

#include "ff_fsa.h"

// the type-erased interface
/*
struct FsaFeatureFunction {
  virtual int markov_order() const = 0;
  virtual ~FsaFeatureFunction();

};

// conforming to above interface, type erases FsaImpl
// you might be wondering: why do this?  answer: it's cool, and it means that the bottom-up ff over ff_fsa wrapper doesn't go through multiple layers of dynamic dispatch
template <class Impl>
struct FsaFeatureFunctionDynamic : public FsaFeatureFunction {
  Impl& d() { return static_cast<Impl&>(*this); }
  Impl const& d() { return static_cast<Impl const&>(*this); }
  int markov_order() const { return d().markov_order(); }
};

//TODO: wrap every method in concrete fsaff and declare in interface above.
//TODO: combine 2 (or N) FsaFeatureFunction (type erased)

*/


#endif
