#ifndef FF_LM_FSA_H
#define FF_LM_FSA_H

#include "ff_lm.h"
#include "ff_from_fsa.h"

class LanguageModelFsa : public FsaFeatureFunctionBase {
  static std::string usage(bool,bool);
  LanguageModelFsa(std::string const& param);
  // implementations in ff_lm.cc
};

typedef FeatureFunctionFromFsa<LanguageModelFsa> LanguageModelFromFsa;

#endif
