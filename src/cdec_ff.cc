#include <boost/shared_ptr.hpp>

#include "ff.h"
#include "lm_ff.h"
#include "ff_factory.h"
#include "ff_wordalign.h"

boost::shared_ptr<FFRegistry> global_ff_registry;

void register_feature_functions() {
  global_ff_registry->Register("LanguageModel", new FFFactory<LanguageModel>);
  global_ff_registry->Register("WordPenalty", new FFFactory<WordPenalty>);
  global_ff_registry->Register("RelativeSentencePosition", new FFFactory<RelativeSentencePosition>);
  global_ff_registry->Register("MarkovJump", new FFFactory<MarkovJump>);
  global_ff_registry->Register("BlunsomSynchronousParseHack", new FFFactory<BlunsomSynchronousParseHack>);
  global_ff_registry->Register("AlignerResults", new FFFactory<AlignerResults>);
};

