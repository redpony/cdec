#include <boost/shared_ptr.hpp>

#include "ff.h"
#include "ff_lm.h"
#include "ff_csplit.h"
#include "ff_wordalign.h"
#include "ff_tagger.h"
#include "ff_factory.h"
#include "ff_ruleshape.h"
#include "ff_bleu.h"
#include "ff_sample_fsa.h"

boost::shared_ptr<FFRegistry> global_ff_registry;

void register_feature_functions() {
  global_ff_registry->Register(new FFFactory<LanguageModel>);
  global_ff_registry->Register(new FFFactory<WordPenaltyFromFsa>); // same as WordPenalty, but implemented using ff_fsa
  global_ff_registry->Register(new FFFactory<FeatureFunctionFromFsa<LongerThanPrev> >);
  global_ff_registry->Register(new FFFactory<FeatureFunctionFromFsa<ShorterThanPrev> >);
  //TODO: use for all features the new Register which requires usage(...)
#ifdef HAVE_RANDLM
  global_ff_registry->Register("RandLM", new FFFactory<LanguageModelRandLM>);
#endif
  global_ff_registry->Register(new FFFactory<WordPenalty>);
  global_ff_registry->Register(new FFFactory<SourceWordPenalty>);
  global_ff_registry->Register(new FFFactory<ArityPenalty>);
  global_ff_registry->Register(new FFFactory<BLEUModel>);
  global_ff_registry->Register("RuleShape", new FFFactory<RuleShapeFeatures>);
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
}
