#ifndef FF_FSA_REGISTER_H
#define FF_FSA_REGISTER_H

#include "ff_factory.h"

template <class Impl>
inline void RegisterFF() {
  ff_registry.Register(new FFFactory<Impl>);
}

void register_feature_functions();

#endif
