#include <boost/shared_ptr.hpp>

#include "ff.h"
#include "ff_lm.h"
#include "ff_csplit.h"
#include "ff_wordalign.h"
#include "ff_tagger.h"
#include "ff_factory.h"
#include "ff_ruleshape.h"
#include "ff_bleu.h"
#include "ff_lm_fsa.h"
#include "ff_sample_fsa.h"
#include "ff_register.h"

void register_feature_functions() {
  RegisterFF<LanguageModel>();
  RegisterFsaImpl<LanguageModelFsa>(true,false); // same as LM but using fsa wrapper

  RegisterFF<WordPenalty>();
  RegisterFF<SourceWordPenalty>();
  RegisterFF<ArityPenalty>();
  RegisterFF<BLEUModel>();

  //TODO: worthless example target FSA ffs.  remove later
  ff_registry.Register(new FFFactory<WordPenaltyFromFsa>); // same as WordPenalty, but implemented using ff_fsa
  ff_registry.Register(new FFFactory<FeatureFunctionFromFsa<SameFirstLetter> >);
  ff_registry.Register(new FFFactory<FeatureFunctionFromFsa<LongerThanPrev> >);
  ff_registry.Register(new FFFactory<FeatureFunctionFromFsa<ShorterThanPrev> >);

  //TODO: use for all features the new Register which requires static FF::usage(false,false) give name
#ifdef HAVE_RANDLM
  ff_registry.Register("RandLM", new FFFactory<LanguageModelRandLM>);
#endif
  ff_registry.Register("RuleShape", new FFFactory<RuleShapeFeatures>);
  ff_registry.Register("RelativeSentencePosition", new FFFactory<RelativeSentencePosition>);
  ff_registry.Register("Model2BinaryFeatures", new FFFactory<Model2BinaryFeatures>);
  ff_registry.Register("MarkovJump", new FFFactory<MarkovJump>);
  ff_registry.Register("MarkovJumpFClass", new FFFactory<MarkovJumpFClass>);
  ff_registry.Register("SourcePOSBigram", new FFFactory<SourcePOSBigram>);
  ff_registry.Register("BlunsomSynchronousParseHack", new FFFactory<BlunsomSynchronousParseHack>);
  ff_registry.Register("AlignerResults", new FFFactory<AlignerResults>);
  ff_registry.Register("CSplit_BasicFeatures", new FFFactory<BasicCSplitFeatures>);
  ff_registry.Register("CSplit_ReverseCharLM", new FFFactory<ReverseCharLMCSplitFeature>);
  ff_registry.Register("Tagger_BigramIdentity", new FFFactory<Tagger_BigramIdentity>);
  ff_registry.Register("LexicalPairIdentity", new FFFactory<LexicalPairIdentity>);

}

