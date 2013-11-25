bin_PROGRAMS = cdec

noinst_PROGRAMS = \
  trule_test \
  hg_test \
  parser_test \
  grammar_test

TESTS = trule_test parser_test grammar_test hg_test
parser_test_SOURCES = parser_test.cc
parser_test_LDADD = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) libcdec.a ../mteval/libmteval.a ../utils/libutils.a
grammar_test_SOURCES = grammar_test.cc
grammar_test_LDADD = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) libcdec.a ../mteval/libmteval.a ../utils/libutils.a
hg_test_SOURCES = hg_test.cc
hg_test_LDADD = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) libcdec.a ../mteval/libmteval.a ../utils/libutils.a
trule_test_SOURCES = trule_test.cc
trule_test_LDADD = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) libcdec.a ../mteval/libmteval.a ../utils/libutils.a

cdec_SOURCES = cdec.cc
cdec_LDFLAGS= -rdynamic
cdec_LDADD = libcdec.a ../mteval/libmteval.a ../utils/libutils.a ../klm/search/libksearch.a ../klm/lm/libklm.a ../klm/util/libklm_util.a ../klm/util/double-conversion/libklm_util_double.a

AM_CPPFLAGS = -DTEST_DATA=\"$(top_srcdir)/decoder/test_data\" -DBOOST_TEST_DYN_LINK -W -Wno-sign-compare -I$(top_srcdir) -I$(top_srcdir)/mteval -I$(top_srcdir)/utils -I$(top_srcdir)/klm

rule_lexer.cc: rule_lexer.ll
	$(LEX) -s -CF -8 -o$@ $<

noinst_LIBRARIES = libcdec.a

EXTRA_DIST = test_data rule_lexer.ll

libcdec_a_SOURCES = \
  JSON_parser.h \
  aligner.h \
  apply_models.h \
  bottom_up_parser.h \
  csplit.h \
  decoder.h \
  earley_composer.h \
  exp_semiring.h \
  factored_lexicon_helper.h \
  ff.h \
  ff_basic.h \
  ff_bleu.h \
  ff_charset.h \
  ff_context.h \
  ff_csplit.h \
  ff_external.h \
  ff_factory.h \
  ff_klm.h \
  ff_lm.h \
  ff_ngrams.h \
  ff_parse_match.h \
  ff_register.h \
  ff_rules.h \
  ff_ruleshape.h \
  ff_sample_fsa.h \
  ff_soft_syntax.h \
  ff_soft_syntax_mindist.h \
  ff_source_path.h \
  ff_source_syntax.h \
  ff_source_syntax2.h \
  ff_spans.h \
  ff_tagger.h \
  ff_wordalign.h \
  ff_wordset.h \
  ffset.h \
  forest_writer.h \
  freqdict.h \
  grammar.h \
  hg.h \
  hg_intersect.h \
  hg_io.h \
  hg_remove_eps.h \
  hg_sampler.h \
  hg_test.h \
  hg_union.h \
  incremental.h \
  inside_outside.h \
  json_parse.h \
  kbest.h \
  lattice.h \
  lexalign.h \
  lextrans.h \
  nt_span.h \
  oracle_bleu.h \
  phrasebased_translator.h \
  phrasetable_fst.h \
  program_options.h \
  rule_lexer.h \
  sentence_metadata.h \
  sentences.h \
  tagger.h \
  translator.h \
  trule.h \
  viterbi.h \
  aligner.cc \
  apply_models.cc \
  bottom_up_parser.cc \
  cdec.cc \
  cdec_ff.cc \
  csplit.cc \
  decoder.cc \
  earley_composer.cc \
  factored_lexicon_helper.cc \
  ff.cc \
  ff_basic.cc \
  ff_bleu.cc \
  ff_charset.cc \
  ff_context.cc \
  ff_csplit.cc \
  ff_external.cc \
  ff_factory.cc \
  ff_klm.cc \
  ff_lm.cc \
  ff_ngrams.cc \
  ff_parse_match.cc \
  ff_rules.cc \
  ff_ruleshape.cc \
  ff_soft_syntax.cc \
  ff_soft_syntax_mindist.cc \
  ff_source_path.cc \
  ff_source_syntax.cc \
  ff_source_syntax2.cc \
  ff_spans.cc \
  ff_tagger.cc \
  ff_wordalign.cc \
  ff_wordset.cc \
  ffset.cc \
  forest_writer.cc \
  fst_translator.cc \
  grammar.cc \
  hg.cc \
  hg_intersect.cc \
  hg_io.cc \
  hg_remove_eps.cc \
  hg_sampler.cc \
  hg_union.cc \
  incremental.cc \
  json_parse.cc \
  lattice.cc \
  lexalign.cc \
  lextrans.cc \
  maxtrans_blunsom.cc \
  phrasebased_translator.cc \
  phrasetable_fst.cc \
  rescore_translator.cc \
  rule_lexer.cc \
  scfg_translator.cc \
  tagger.cc \
  translator.cc \
  trule.cc \
  viterbi.cc \
  JSON_parser.c
