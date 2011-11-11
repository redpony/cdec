bin_PROGRAMS = cdec

if HAVE_GTEST
noinst_PROGRAMS = \
  trule_test \
  hg_test \
  ff_test \
  parser_test \
  grammar_test
 
 # cfg_test
TESTS = trule_test ff_test parser_test grammar_test hg_test
# cfg_test
#cfg_test_SOURCES = cfg_test.cc
#cfg_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a ../mteval/libmteval.a ../utils/libutils.a -lz
parser_test_SOURCES = parser_test.cc
parser_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a ../mteval/libmteval.a ../utils/libutils.a -lz
ff_test_SOURCES = ff_test.cc
ff_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a ../mteval/libmteval.a ../utils/libutils.a -lz
grammar_test_SOURCES = grammar_test.cc
grammar_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a ../mteval/libmteval.a ../utils/libutils.a -lz
hg_test_SOURCES = hg_test.cc
hg_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a ../mteval/libmteval.a ../utils/libutils.a -lz
trule_test_SOURCES = trule_test.cc
trule_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a ../mteval/libmteval.a ../utils/libutils.a -lz
endif

cdec_SOURCES = cdec.cc
cdec_LDADD = libcdec.a ../mteval/libmteval.a ../utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

AM_CPPFLAGS = -W -Wno-sign-compare $(GTEST_CPPFLAGS) -I.. -I../mteval -I../utils -I../klm

rule_lexer.cc: rule_lexer.l
	$(LEX) -s -CF -8 -o$@ $<

noinst_LIBRARIES = libcdec.a

libcdec_a_SOURCES = \
  forest_writer.cc \
  maxtrans_blunsom.cc \
  cdec_ff.cc \
  cfg.cc \
  dwarf.cc \
  ff_dwarf.cc \
  rule_lexer.cc \
  fst_translator.cc \
  csplit.cc \
  translator.cc \
  scfg_translator.cc \
  hg.cc \
  hg_io.cc \
  decoder.cc \
  hg_intersect.cc \
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
  ff_rules.cc \
  ff_wordset.cc \
  ff_charset.cc \
  ff_lm.cc \
  ff_klm.cc \
  ff_ngrams.cc \
  ff_spans.cc \
  ff_ruleshape.cc \
  ff_wordalign.cc \
  ff_csplit.cc \
  ff_tagger.cc \
  ff_source_syntax.cc \
  ff_bleu.cc \
  ff_factory.cc \
  freqdict.cc \
  lexalign.cc \
  lextrans.cc \
  tagger.cc \
  bottom_up_parser.cc \
  phrasebased_translator.cc \
  JSON_parser.c \
  json_parse.cc \
  grammar.cc

if GLC
  # Until we build GLC as a library...
  libcdec_a_SOURCES += ff_glc.cc string_util.cc feature-factory.cc
endif
