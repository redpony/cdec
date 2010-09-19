#ifndef FF_FSA_REGISTER_H
#define FF_FSA_REGISTER_H

#include "ff_factory.h"
#include "ff_from_fsa.h"
#include "ff_fsa_dynamic.h"

inline std::string prefix_fsa(std::string const& name,bool fsa_prefix_ff) {
  return fsa_prefix_ff ? "Fsa"+name : name;
}

//FIXME: problem with FeatureFunctionFromFsa<FsaFeatureFunction> - need to use factory rather than ctor.
#if 0
template <class DynFsa>
inline void RegisterFsa(bool ff_also=true,bool fsa_prefix_ff=true) {
  assert(!ff_also);
//  global_fsa_ff_registry->RegisterFsa<DynFsa>();
//if (ff_also) ff_registry.RegisterFF<FeatureFunctionFromFsa<DynFsa> >(prefix_fsa(DynFsa::usage(false,false)),fsa_prefix_ff);
}
#endif

//TODO: ff from fsa that uses pointer to fsa impl?  e.g. in LanguageModel we share underlying lm file by recognizing same param, but without that effort, otherwise stateful ff may duplicate state if we enable both fsa and ff_from_fsa
template <class FsaImpl>
inline void RegisterFsaImpl(bool ff_also=true,bool fsa_prefix_ff=false) {
  typedef FsaFeatureFunctionDynamic<FsaImpl> DynFsa;
  typedef FeatureFunctionFromFsa<FsaImpl> FFFrom;
  std::string name=FsaImpl::usage(false,false);
  fsa_ff_registry.Register(new FsaFactory<DynFsa>);
  if (ff_also)
    ff_registry.Register(prefix_fsa(name,fsa_prefix_ff),new FFFactory<FFFrom>);
}

template <class Impl>
inline void RegisterFF() {
  ff_registry.Register(new FFFactory<Impl>);
}

template <class FsaImpl>
inline void RegisterFsaDynToFF(std::string name,bool prefix=true) {
  typedef FsaFeatureFunctionDynamic<FsaImpl> DynFsa;
  ff_registry.Register(prefix?"DynamicFsa"+name:name,new FFFactory<FeatureFunctionFromFsa<DynFsa> >);
}

template <class FsaImpl>
inline void RegisterFsaDynToFF(bool prefix=true) {
  RegisterFsaDynToFF<FsaImpl>(FsaImpl::usage(false,false),prefix);
}

void register_feature_functions();

#endif
