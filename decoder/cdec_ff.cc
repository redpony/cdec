#include <boost/shared_ptr.hpp>

#include "ff.h"
#include "ff_spans.h"
#include "ff_lm.h"
#include "ff_klm.h"
#include "ff_ngrams.h"
#include "ff_csplit.h"
#include "ff_wordalign.h"
#include "ff_tagger.h"
#include "ff_factory.h"
#include "ff_ruleshape.h"
#include "ff_bleu.h"
#include "ff_lm_fsa.h"
#include "ff_sample_fsa.h"
#include "ff_register.h"
#include "ff_charset.h"
#include "ff_wordset.h"
#include "ff_dwarf.h"

#ifdef HAVE_GLC
#include <cdec/ff_glc.h>
#endif

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
  ff_registry.Register("SpanFeatures", new FFFactory<SpanFeatures>());
  ff_registry.Register("NgramFeatures", new FFFactory<NgramDetector>());
  ff_registry.Register("RuleNgramFeatures", new FFFactory<RuleNgramFeatures>());
  ff_registry.Register("CMR2008ReorderingFeatures", new FFFactory<CMR2008ReorderingFeatures>());
  ff_registry.Register("KLanguageModel", new FFFactory<KLanguageModel<lm::ngram::ProbingModel> >());
  ff_registry.Register("KLanguageModel_Trie", new FFFactory<KLanguageModel<lm::ngram::TrieModel> >());
  ff_registry.Register("KLanguageModel_QuantTrie", new FFFactory<KLanguageModel<lm::ngram::QuantTrieModel> >());
  ff_registry.Register("KLanguageModel_Probing", new FFFactory<KLanguageModel<lm::ngram::ProbingModel> >());
  ff_registry.Register("NonLatinCount", new FFFactory<NonLatinCount>);
  ff_registry.Register("RuleShape", new FFFactory<RuleShapeFeatures>);
  ff_registry.Register("RelativeSentencePosition", new FFFactory<RelativeSentencePosition>);
  ff_registry.Register("LexNullJump", new FFFactory<LexNullJump>);
  ff_registry.Register("NewJump", new FFFactory<NewJump>);
  ff_registry.Register("SourceBigram", new FFFactory<SourceBigram>);
  ff_registry.Register("Fertility", new FFFactory<Fertility>);
  ff_registry.Register("BlunsomSynchronousParseHack", new FFFactory<BlunsomSynchronousParseHack>);
  ff_registry.Register("CSplit_BasicFeatures", new FFFactory<BasicCSplitFeatures>);
  ff_registry.Register("CSplit_ReverseCharLM", new FFFactory<ReverseCharLMCSplitFeature>);
  ff_registry.Register("Tagger_BigramIndicator", new FFFactory<Tagger_BigramIndicator>);
  ff_registry.Register("LexicalPairIndicator", new FFFactory<LexicalPairIndicator>);
  ff_registry.Register("OutputIndicator", new FFFactory<OutputIndicator>);
  ff_registry.Register("IdentityCycleDetector", new FFFactory<IdentityCycleDetector>);
  ff_registry.Register("InputIndicator", new FFFactory<InputIndicator>);
  ff_registry.Register("LexicalTranslationTrigger", new FFFactory<LexicalTranslationTrigger>);
  ff_registry.Register("WordPairFeatures", new FFFactory<WordPairFeatures>);
  ff_registry.Register("WordSet", new FFFactory<WordSet>);
  ff_registry.Register("Dwarf", new FFFactory<Dwarf>);
#ifdef HAVE_GLC
  ff_registry.Register("ContextCRF", new FFFactory<Model1Features>);
#endif
}

