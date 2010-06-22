#include <boost/shared_ptr.hpp>

#include "ff.h"
#include "ff_lm.h"
#include "ff_csplit.h"
#include "ff_wordalign.h"
#include "ff_tagger.h"
#include "ff_factory.h"

boost::shared_ptr<FFRegistry> global_ff_registry;

void register_feature_functions() {
  global_ff_registry->Register("LanguageModel", new FFFactory<LanguageModel>);
#ifdef HAVE_RANDLM
  global_ff_registry->Register("RandLM", new FFFactory<LanguageModelRandLM>);
#endif
  global_ff_registry->Register("WordPenalty", new FFFactory<WordPenalty>);
  global_ff_registry->Register("SourceWordPenalty", new FFFactory<SourceWordPenalty>);
  global_ff_registry->Register("ArityPenalty", new FFFactory<ArityPenalty>);
  global_ff_registry->Register("RelativeSentencePosition", new FFFactory<RelativeSentencePosition>);
  global_ff_registry->Register("Model2BinaryFeatures", new FFFactory<Model2BinaryFeatures>);
  global_ff_registry->Register("MarkovJump", new FFFactory<MarkovJump>);
  global_ff_registry->Register("MarkovJumpFClass", new FFFactory<MarkovJumpFClass>);
  global_ff_registry->Register("SourcePOSBigram", new FFFactory<SourcePOSBigram>);
  global_ff_registry->Register("BlunsomSynchronousParseHack", new FFFactory<BlunsomSynchronousParseHack>);
  global_ff_registry->Register("AlignerResults", new FFFactory<AlignerResults>);
  global_ff_registry->Register("CSplit_BasicFeatures", new FFFactory<BasicCSplitFeatures>);
  global_ff_registry->Register("CSplit_ReverseCharLM", new FFFactory<ReverseCharLMCSplitFeature>);
  global_ff_registry->Register("Tagger_BigramIdentity", new FFFactory<Tagger_BigramIdentity>);
  global_ff_registry->Register("LexicalPairIdentity", new FFFactory<LexicalPairIdentity>);
};

