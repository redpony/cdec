bin_PROGRAMS = \
  dict_test \
  weights_test \
  trule_test \
  hg_test \
  ff_test \
  parser_test \
  grammar_test \
  cdec \
  small_vector_test

cdec_SOURCES = cdec.cc forest_writer.cc maxtrans_blunsom.cc cdec_ff.cc ff_factory.cc timing_stats.cc
small_vector_test_SOURCES = small_vector_test.cc
small_vector_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libhg.a
parser_test_SOURCES = parser_test.cc
parser_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libhg.a
dict_test_SOURCES = dict_test.cc
dict_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libhg.a
ff_test_SOURCES = ff_test.cc
ff_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libhg.a
grammar_test_SOURCES = grammar_test.cc
grammar_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libhg.a
hg_test_SOURCES = hg_test.cc
hg_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libhg.a
trule_test_SOURCES = trule_test.cc
trule_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libhg.a
weights_test_SOURCES = weights_test.cc
weights_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libhg.a

LDADD = libhg.a

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LIB) -lz

noinst_LIBRARIES = libhg.a

libhg_a_SOURCES = \
  fst_translator.cc \
  scfg_translator.cc \
  hg.cc \
  hg_io.cc \
  viterbi.cc \
  lattice.cc \
  logging.cc \
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
  lm_ff.cc \
  ff_wordalign.cc \
  hg_intersect.cc \
  lexcrf.cc \
  bottom_up_parser.cc \
  phrasebased_translator.cc \
  JSON_parser.c \
  json_parse.cc \
  grammar.cc
