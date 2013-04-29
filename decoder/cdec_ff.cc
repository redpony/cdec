#include <boost/shared_ptr.hpp>

#include "ff.h"
#include "ff_basic.h"
#include "ff_context.h"
#include "ff_spans.h"
#include "ff_lm.h"
#include "ff_klm.h"
#include "ff_ngrams.h"
#include "ff_csplit.h"
#include "ff_wordalign.h"
#include "ff_tagger.h"
#include "ff_factory.h"
#include "ff_rules.h"
#include "ff_ruleshape.h"
#include "ff_bleu.h"
#include "ff_soft_syntax.h"
#include "ff_soft_syntax2.h"
#include "ff_source_path.h"


#include "ff_parse_match.h"
#include "ff_source_syntax.h"
#include "ff_source_syntax_p.h"
#include "ff_source_syntax2.h"
#include "ff_source_syntax2_p.h"


#include "ff_register.h"
#include "ff_charset.h"
#include "ff_wordset.h"
#include "ff_dwarf.h"
#include "ff_external.h"

#ifdef HAVE_GLC
#include <cdec/ff_glc.h>
#endif

void register_feature_functions() {
  static bool registered = false;
  if (registered) {
    assert(!"register_feature_functions() called twice!");
  }
  registered = true;

  RegisterFF<LanguageModel>();

  RegisterFF<WordPenalty>();
  RegisterFF<SourceWordPenalty>();
  RegisterFF<ArityPenalty>();
  RegisterFF<BLEUModel>();

  //TODO: use for all features the new Register which requires static FF::usage(false,false) give name
#ifdef HAVE_RANDLM
  ff_registry.Register("RandLM", new FFFactory<LanguageModelRandLM>);
#endif
  ff_registry.Register("SpanFeatures", new FFFactory<SpanFeatures>());
  ff_registry.Register("NgramFeatures", new FFFactory<NgramDetector>());
  ff_registry.Register("RuleContextFeatures", new FFFactory<RuleContextFeatures>());
  ff_registry.Register("RuleIdentityFeatures", new FFFactory<RuleIdentityFeatures>());


  ff_registry.Register("ParseMatchFeatures", new FFFactory<ParseMatchFeatures>);

  ff_registry.Register("SoftSyntacticFeatures", new FFFactory<SoftSyntacticFeatures>);
  ff_registry.Register("SoftSyntacticFeatures2", new FFFactory<SoftSyntacticFeatures2>);

  ff_registry.Register("SourceSyntaxFeatures", new FFFactory<SourceSyntaxFeatures>);
  ff_registry.Register("SourceSyntaxFeatures2", new FFFactory<SourceSyntaxFeatures2>);

  ff_registry.Register("SourceSpanSizeFeatures", new FFFactory<SourceSpanSizeFeatures>);

  //ff_registry.Register("PSourceSyntaxFeatures", new FFFactory<PSourceSyntaxFeatures>);
  //ff_registry.Register("PSourceSpanSizeFeatures", new FFFactory<PSourceSpanSizeFeatures>);
  //ff_registry.Register("PSourceSyntaxFeatures2", new FFFactory<PSourceSyntaxFeatures2>);


  ff_registry.Register("CMR2008ReorderingFeatures", new FFFactory<CMR2008ReorderingFeatures>());
  ff_registry.Register("RuleSourceBigramFeatures", new FFFactory<RuleSourceBigramFeatures>());
  ff_registry.Register("RuleTargetBigramFeatures", new FFFactory<RuleTargetBigramFeatures>());
  ff_registry.Register("KLanguageModel", new KLanguageModelFactory());
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
  ff_registry.Register("SourcePathFeatures", new FFFactory<SourcePathFeatures>);
  ff_registry.Register("WordSet", new FFFactory<WordSet>);
  ff_registry.Register("Dwarf", new FFFactory<Dwarf>);
  ff_registry.Register("External", new FFFactory<ExternalFeature>);
#ifdef HAVE_GLC
  ff_registry.Register("ContextCRF", new FFFactory<Model1Features>);
#endif
}

