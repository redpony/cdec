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
  static bool registered = false;
  if (registered) {
    assert(!"register_feature_functions() called twice!");
  }
  registered = true;

  //TODO: these are worthless example target FSA ffs.  remove later
  RegisterFsaImpl<SameFirstLetter>(true);
  RegisterFsaImpl<LongerThanPrev>(true);
  RegisterFsaImpl<ShorterThanPrev>(true);
//  ff_registry.Register("LanguageModelFsaDynamic",new FFFactory<FeatureFunctionFromFsa<FsaFeatureFunctionDynamic<LanguageModelFsa> > >); // to test correctness of FsaFeatureFunctionDynamic erasure
  RegisterFsaDynToFF<LanguageModelFsa>();
  RegisterFsaImpl<LanguageModelFsa>(true); // same as LM but using fsa wrapper
  RegisterFsaDynToFF<SameFirstLetter>();

  RegisterFF<LanguageModel>();

  RegisterFF<WordPenalty>();
  RegisterFF<SourceWordPenalty>();
  RegisterFF<ArityPenalty>();
  RegisterFF<BLEUModel>();

  ff_registry.Register(new FFFactory<WordPenaltyFromFsa>); // same as WordPenalty, but implemented using ff_fsa

  //TODO: use for all features the new Register which requires static FF::usage(false,false) give name
#ifdef HAVE_RANDLM
  ff_registry.Register("RandLM", new FFFactory<LanguageModelRandLM>);
#endif
  ff_registry.Register("RuleShape", new FFFactory<RuleShapeFeatures>);
  ff_registry.Register("RelativeSentencePosition", new FFFactory<RelativeSentencePosition>);
  ff_registry.Register("Model2BinaryFeatures", new FFFactory<Model2BinaryFeatures>);
  ff_registry.Register("MarkovJump", new FFFactory<MarkovJump>);
  ff_registry.Register("MarkovJumpFClass", new FFFactory<MarkovJumpFClass>);
  ff_registry.Register("SourceBigram", new FFFactory<SourceBigram>);
  ff_registry.Register("SourcePOSBigram", new FFFactory<SourcePOSBigram>);
  ff_registry.Register("BlunsomSynchronousParseHack", new FFFactory<BlunsomSynchronousParseHack>);
  ff_registry.Register("AlignerResults", new FFFactory<AlignerResults>);
  ff_registry.Register("CSplit_BasicFeatures", new FFFactory<BasicCSplitFeatures>);
  ff_registry.Register("CSplit_ReverseCharLM", new FFFactory<ReverseCharLMCSplitFeature>);
  ff_registry.Register("Tagger_BigramIdentity", new FFFactory<Tagger_BigramIdentity>);
  ff_registry.Register("LexicalPairIdentity", new FFFactory<LexicalPairIdentity>);

}

