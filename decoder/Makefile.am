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
cdec_LDADD = libcdec.a ../mteval/libmteval.a ../utils/libutils.a ../klm/search/libksearch.a ../klm/lm/libklm.a ../klm/util/libklm_util.a ../klm/util/double-conversion/libklm_util_double.a

AM_CPPFLAGS = -DTEST_DATA=\"$(top_srcdir)/decoder/test_data\" -DBOOST_TEST_DYN_LINK -W -Wno-sign-compare -I$(top_srcdir) -I$(top_srcdir)/mteval -I$(top_srcdir)/utils -I$(top_srcdir)/klm

rule_lexer.cc: rule_lexer.ll
	$(LEX) -s -CF -8 -o$@ $<

noinst_LIBRARIES = libcdec.a

EXTRA_DIST = test_data rule_lexer.ll

libcdec_a_SOURCES = \
  JSON_parser.h \
  aligner.h \
  apply_fsa_models.h \
  apply_models.h \
  bottom_up_parser.h \
  cfg.h \
  cfg_binarize.h \
  cfg_format.h \
  cfg_options.h \
  csplit.h \
  decoder.h \
  dwarf.h \
  earley_composer.h \
  exp_semiring.h \
  factored_lexicon_helper.h \
  ff.h \
  ff_basic.h \
  ff_bleu.h \
  ff_charset.h \
  ff_context.h \
  ff_csplit.h \
  ff_dwarf.h \
  ff_external.h \
  ff_factory.h \
  ff_klm.h \
  ff_lm.h \
  ff_ngrams.h \
  ff_register.h \
  ff_rules.h \
  ff_ruleshape.h \
  ff_sample_fsa.h \
  ff_source_path.h \
  ff_source_syntax.h \
  ff_spans.h \
  ff_tagger.h \
  ff_wordalign.h \
  ff_wordset.h \
  ffset.h \
  forest_writer.h \
  freqdict.h \
  grammar.h \
  hg.h \
  hg_cfg.h \
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
  tromble_loss.h \
  trule.h \
  viterbi.h \
  forest_writer.cc \
  maxtrans_blunsom.cc \
  cdec_ff.cc \
  cfg.cc \
  dwarf.cc \
  ff_dwarf.cc \
  ff_external.cc \
  rule_lexer.cc \
  fst_translator.cc \
  csplit.cc \
  translator.cc \
  rescore_translator.cc \
  scfg_translator.cc \
  hg.cc \
  hg_io.cc \
  hg_remove_eps.cc \
  decoder.cc \
  hg_intersect.cc \
  hg_union.cc \
  hg_sampler.cc \
  factored_lexicon_helper.cc \
  viterbi.cc \
  lattice.cc \
  aligner.cc \
  apply_models.cc \
  earley_composer.cc \
  phrasetable_fst.cc \
  trule.cc \
  ff.cc \
  ffset.cc \
  ff_basic.cc \
  ff_rules.cc \
  ff_wordset.cc \
  ff_context.cc \
  ff_charset.cc \
  ff_lm.cc \
  ff_klm.cc \
  ff_ngrams.cc \
  ff_spans.cc \
  ff_ruleshape.cc \
  ff_wordalign.cc \
  ff_csplit.cc \
  ff_tagger.cc \
  ff_source_path.cc \
  ff_parse_match.cc \
	ff_soft_syntax.cc \
	ff_soft_syntax2.cc \
  ff_source_syntax.cc \
	ff_source_syntax_p.cc \
  ff_source_syntax2.cc \
  ff_source_syntax2_p.cc \
  ff_bleu.cc \
  ff_factory.cc \
  incremental.cc \
  lexalign.cc \
  lextrans.cc \
  tagger.cc \
  bottom_up_parser.cc \
  phrasebased_translator.cc \
  JSON_parser.c \
  json_parse.cc \
  grammar.cc
