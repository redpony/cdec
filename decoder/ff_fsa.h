#ifndef FF_FSA_H
#define FF_FSA_H

//TODO: actually compile this; probably full of syntax errors.

#include <stdint.h> //C99
#include <string>
#include "ff.h"
#include "sparse_vector.h"
#include "value_array.h" // used to hold state
#include "tdict.h"
#include "hg.h"

typedef ValueArray<uint8_t> Bytes;

/*
  features whose score is just some PFSA over target string.  TODO: could decide to give access to source span of scanned words as well if someone devises a feature that can use it

  state is some fixed width byte array.  could actually be a void *, WordID sequence, whatever.

 */

// it's not necessary to inherit from this.
struct FsaFeatureFunctionBase {
  std::string name,usage_short,usage_verbose;
  int fid; // you can have more than 1 feature of course.
  void InitFid() { // call this, though, if you have a single feature
    fid=FD::Convert(name);
  }
  std::string usage(bool param,bool verbose) {
    return FeatureFunction::usage_helper(name,usage_short,usage_verbose,param,verbose);
  }

  FsaFeatureFunctionBase(std::string const& name,std::string const& usage_verbose="[no documentation yet]",std::string const& usage_short="[no parameters]") : name(name),usage_short(usage_short),usage_verbose(usage_verbose) {  }

  int state_bytes; // don't forget to set this (it may depend on params of course)
};

// example: feature val = -1 * # of target words
struct TargetPenaltyFsa : public FsaFeatureFunctionBase {
  TargetPenaltyFsa(std::string const& param) : FsaFeatureFunctionBase("TargetPenalty","","-1 per target word") { InitFid(); }
  const float val_per_target_word=-1;
  // state for backoff

  // scan
  void Scan(SentenceMetadata const& smeta,WordID x,void const* prev_state,FeatureVector *features) {
    features->set_value(fid,val_per_target_word);
  }

  // heuristic estimate of phrase
  void Heuristic(WordID const* begin, WordID const* end,FeatureVector *h_features)

  // return m: all strings x with the same final m+1 letters must end in this state
  /* markov chain of order m: P(xn|xn-1...x1)=P(xn|xn-1...xn-m) */
  int MarkovOrder() const {
    return 0;
  }

};

//TODO: combine 2 FsaFeatures typelist style (can recurse for more)

// the type-erased interface
struct FsaFeatureFunction {
  virtual int MarkovOrder() const = 0;
  virtual ~FsaFeatureFunction();

};

// conforming to above interface, type erases FsaImpl
// you might be wondering: why do this?  answer: it's cool, and it means that the bottom-up ff over ff_fsa wrapper doesn't go through multiple layers of dynamic dispatch
template <class Impl>
struct FsaFeatureFunctionDynamic : public FsaFeatureFunction {
  Impl& d() { return static_cast<Impl&>(*this); }
  Impl const& d() { return static_cast<Impl const&>(*this); }
  int MarkovOrder() const { return d().MarkovOrder(); }
};

//TODO: combine 2 (or N) FsaFeatureFunction (type erased)

/* regular bottom up scorer from Fsa feature
   uses guarantee about markov order=N to score ASAP
   encoding of state: if less than N-1 (ctxlen) words

   either:
   struct FF : public FsaImpl,FeatureFunctionFromFsa<FF> (more efficient)

   or:
   struct FF : public FsaFeatureFunctionDynamic,FeatureFunctionFromFsa<FF> (code sharing, but double dynamic dispatch)
 */

template <class Impl>
struct FeatureFunctionFromFsa : public FeatureFunction {
  Impl& d() { return static_cast<Impl&>(*this); }
  Impl const& d() { return static_cast<Impl const&>(*this); }
  int M; // markov order (ctx len)
  FeatureFunctionFromFsa() {  }
  Init() {
    name=d().name;
    M=d().MarkovOrder
    SetStateSize(sizeof(WordID)*2*M);
  } // can't do this in constructor because we come before d() in order

  virtual Features Features() const { return d().Features(); }
  bool rule_feature() const {
    return StateSize()==0; // Fsa features don't get info about span
  }

};


#endif
