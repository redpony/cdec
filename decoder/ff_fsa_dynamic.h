#ifndef FF_FSA_DYNAMIC_H
#define FF_FSA_DYNAMIC_H

struct SentenceMetadata;

#include "ff_fsa_data.h"
#include "hg.h" // can't forward declare nested Hypergraph::Edge class
#include <sstream>

// the type-erased interface

//FIXME: diamond inheritance problem.  make a copy of the fixed data?  or else make the dynamic version not wrap but rather be templated CRTP base (yuck)
struct FsaFeatureFunction : public FsaFeatureFunctionData {
  static const bool simple_phrase_score=false;
  virtual int markov_order() const = 0;

  // see ff_fsa.h - FsaFeatureFunctionBase<Impl> gives you reasonable impls of these if you override just ScanAccum
  virtual void ScanAccum(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,
                        WordID w,void const* state,void *next_state,Accum *a) const = 0;
  virtual void ScanPhraseAccum(SentenceMetadata const& smeta,Hypergraph::Edge const & edge,
                              WordID const* i, WordID const* end,
                              void const* state,void *next_state,Accum *accum) const = 0;
  virtual void ScanPhraseAccumOnly(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,
                           WordID const* i, WordID const* end,
                           void const* state,Accum *accum) const = 0;
  virtual void *ScanPhraseAccumBounce(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,WordID const* i, WordID const* end,void *cs,void *ns,Accum *accum) const = 0;

  virtual int early_score_words(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,WordID const* i, WordID const* end,Accum *accum) const { return 0; }
  // called after constructor, before use
  virtual void Init() = 0;
  virtual std::string usage_v(bool param,bool verbose) const {
    return FeatureFunction::usage_helper("unnamed_dynamic_fsa_feature","","",param,verbose);
  }
  virtual void init_name_debug(std::string const& n,bool debug) {
    FsaFeatureFunctionData::init_name_debug(n,debug);
  }

  virtual void print_state(std::ostream &o,void const*state) const {
    FsaFeatureFunctionData::print_state(o,state);
  }
  virtual std::string describe() const { return "[FSA unnamed_dynamic_fsa_feature]"; }

  //end_phrase()
  virtual ~FsaFeatureFunction() {}

  // no need to override:
  std::string describe_state(void const* state) const {
    std::ostringstream o;
    print_state(o,state);
    return o.str();
  }
};

// conforming to above interface, type erases FsaImpl
// you might be wondering: why do this?  answer: it's cool, and it means that the bottom-up ff over ff_fsa wrapper doesn't go through multiple layers of dynamic dispatch
// usage: typedef FsaFeatureFunctionDynamic<MyFsa> MyFsaDyn;
template <class Impl>
struct FsaFeatureFunctionDynamic : public FsaFeatureFunction {
  static const bool simple_phrase_score=Impl::simple_phrase_score;
  Impl& d() { return impl;//static_cast<Impl&>(*this);
  }
  Impl const& d() const { return impl;
    //static_cast<Impl const&>(*this);
  }
  int markov_order() const { return d().markov_order(); }

  std::string describe() const   {
    return d().describe();
  }

  virtual void ScanAccum(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,
                        WordID w,void const* state,void *next_state,Accum *a) const {
    return d().ScanAccum(smeta,edge,w,state,next_state,a);
  }

  virtual void ScanPhraseAccum(SentenceMetadata const& smeta,Hypergraph::Edge const & edge,
                              WordID const* i, WordID const* end,
                              void const* state,void *next_state,Accum *a) const {
    return d().ScanPhraseAccum(smeta,edge,i,end,state,next_state,a);
  }

  virtual void ScanPhraseAccumOnly(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,
                           WordID const* i, WordID const* end,
                           void const* state,Accum *a) const {
    return d().ScanPhraseAccumOnly(smeta,edge,i,end,state,a);
  }

  virtual void *ScanPhraseAccumBounce(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,WordID const* i, WordID const* end,void *cs,void *ns,Accum *a) const {
    return d().ScanPhraseAccumBounce(smeta,edge,i,end,cs,ns,a);
  }

  virtual int early_score_words(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,WordID const* i, WordID const* end,Accum *accum) const {
    return d().early_score_words(smeta,edge,i,end,accum);
  }

  static std::string usage(bool param,bool verbose) {
    return Impl::usage(param,verbose);
  }

  std::string usage_v(bool param,bool verbose) const {
    return Impl::usage(param,verbose);
  }

  virtual void print_state(std::ostream &o,void const*state) const {
    return d().print_state(o,state);
  }

  void init_name_debug(std::string const& n,bool debug) {
    FsaFeatureFunction::init_name_debug(n,debug);
    d().init_name_debug(n,debug);
  }

  virtual void Init() {
    d().sync_to_=(FsaFeatureFunctionData*)this;
    d().Init();
    d().sync();
  }

  template <class I>
  FsaFeatureFunctionDynamic(I const& param) : impl(param) {
    Init();
  }
private:
  Impl impl;
};

// constructor takes ptr or shared_ptr to Impl, otherwise same as above - note: not virtual
template <class Impl>
struct FsaFeatureFunctionPimpl : public FsaFeatureFunctionData {
  typedef boost::shared_ptr<Impl const> Pimpl;
  static const bool simple_phrase_score=Impl::simple_phrase_score;
  Impl const& d() const { return *p_; }
  int markov_order() const { return d().markov_order(); }

  std::string describe() const   {
    return d().describe();
  }

  void ScanAccum(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,
                        WordID w,void const* state,void *next_state,Accum *a) const {
    return d().ScanAccum(smeta,edge,w,state,next_state,a);
  }

  void ScanPhraseAccum(SentenceMetadata const& smeta,Hypergraph::Edge const & edge,
                              WordID const* i, WordID const* end,
                              void const* state,void *next_state,Accum *a) const {
    return d().ScanPhraseAccum(smeta,edge,i,end,state,next_state,a);
  }

  void ScanPhraseAccumOnly(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,
                           WordID const* i, WordID const* end,
                           void const* state,Accum *a) const {
    return d().ScanPhraseAccumOnly(smeta,edge,i,end,state,a);
  }

  void *ScanPhraseAccumBounce(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,WordID const* i, WordID const* end,void *cs,void *ns,Accum *a) const {
    return d().ScanPhraseAccumBounce(smeta,edge,i,end,cs,ns,a);
  }

  int early_score_words(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,WordID const* i, WordID const* end,Accum *accum) const {
    return d().early_score_words(smeta,edge,i,end,accum);
  }

  static std::string usage(bool param,bool verbose) {
    return Impl::usage(param,verbose);
  }

  std::string usage_v(bool param,bool verbose) const {
    return Impl::usage(param,verbose);
  }

  void print_state(std::ostream &o,void const*state) const {
    return d().print_state(o,state);
  }

#if 0
  // this and Init() don't touch p_ because we want to leave the original alone.
      void init_name_debug(std::string const& n,bool debug) {
    FsaFeatureFunctionData::init_name_debug(n,debug);
  }
#endif
  void Init() {
    p_=hold_pimpl_.get();
#if 0
    d().sync_to_=static_cast<FsaFeatureFunctionData*>(this);
    d().Init();
#endif
    *static_cast<FsaFeatureFunctionData*>(this)=d();
  }

  FsaFeatureFunctionPimpl(Impl const* const p) : hold_pimpl_(p,null_deleter()) {
    Init();
  }
  FsaFeatureFunctionPimpl(Pimpl const& p) : hold_pimpl_(p) {
    Init();
  }
private:
  Impl const* p_;
  Pimpl hold_pimpl_;
};

typedef FsaFeatureFunctionPimpl<FsaFeatureFunction> FsaFeatureFunctionFwd; // allow ff_from_fsa for an existing dynamic-type ff (as opposed to usual register a wrapped known-type FSA in ff_register, which is more efficient)
//typedef FsaFeatureFunctionDynamic<FsaFeatureFunctionFwd> DynamicFsaFeatureFunctionFwd;  //if you really need to have a dynamic fsa facade that's also a dynamic fsa

//TODO: combine 2 (or N) FsaFeatureFunction (type erased)


#endif
