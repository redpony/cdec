bin_PROGRAMS = cdec

if HAVE_GTEST
noinst_PROGRAMS = \
  dict_test \
  weights_test \
  trule_test \
  hg_test \
  ff_test \
  logval_test \
  parser_test \
  grammar_test \
  small_vector_test
endif

cdec_SOURCES = cdec.cc forest_writer.cc maxtrans_blunsom.cc cdec_ff.cc ff_factory.cc timing_stats.cc
small_vector_test_SOURCES = small_vector_test.cc
small_vector_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a
parser_test_SOURCES = parser_test.cc
parser_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a
dict_test_SOURCES = dict_test.cc
dict_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a
ff_test_SOURCES = ff_test.cc
ff_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a
grammar_test_SOURCES = grammar_test.cc
grammar_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a
hg_test_SOURCES = hg_test.cc
hg_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a
trule_test_SOURCES = trule_test.cc
trule_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a
weights_test_SOURCES = weights_test.cc
weights_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libcdec.a
logval_test_SOURCES = logval_test.cc
logval_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)

LDADD = libcdec.a

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS)
AM_LDFLAGS = -lz

rule_lexer.cc: rule_lexer.l
	$(LEX) -s -CF -8 -o$@ $<

noinst_LIBRARIES = libcdec.a

libcdec_a_SOURCES = \
  rule_lexer.cc \
  fst_translator.cc \
  csplit.cc \
  translator.cc \
  scfg_translator.cc \
  hg.cc \
  hg_io.cc \
  hg_intersect.cc \
  viterbi.cc \
  lattice.cc \
  aligner.cc \
  gzstream.cc \
  apply_models.cc \
  earley_composer.cc \
  phrasetable_fst.cc \
  sparse_vector.cc \
  trule.cc \
  filelib.cc \
  stringlib.cc \
  fdict.cc \
  tdict.cc \
  weights.cc \
  ttables.cc \
  ff.cc \
  ff_lm.cc \
  ff_wordalign.cc \
  ff_csplit.cc \
  ff_tagger.cc \
  tromble_loss.cc \
  freqdict.cc \
  lexalign.cc \
  lextrans.cc \
  tagger.cc \
  bottom_up_parser.cc \
  phrasebased_translator.cc \
  JSON_parser.c \
  json_parse.cc \
  grammar.cc
