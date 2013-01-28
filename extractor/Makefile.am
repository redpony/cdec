bin_PROGRAMS = compile run_extractor

noinst_PROGRAMS = \
  binary_search_merger_test \
  data_array_test \
  linear_merger_test \
  matching_comparator_test \
  matching_test \
  matchings_finder_test \
  phrase_test \
  precomputation_test \
  suffix_array_test \
  veb_test

TESTS = precomputation_test
#TESTS = binary_search_merger_test \
#  data_array_test \
#  linear_merger_test \
#  matching_comparator_test \
#  matching_test \
#  phrase_test \
#  suffix_array_test \
#  veb_test

binary_search_merger_test_SOURCES = binary_search_merger_test.cc
binary_search_merger_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
data_array_test_SOURCES = data_array_test.cc
data_array_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a
linear_merger_test_SOURCES = linear_merger_test.cc
linear_merger_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
matching_comparator_test_SOURCES = matching_comparator_test.cc
matching_comparator_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a
matching_test_SOURCES = matching_test.cc
matching_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a
matchings_finder_test_SOURCES = matchings_finder_test.cc
matchings_finder_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
phrase_test_SOURCES = phrase_test.cc
phrase_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
precomputation_test_SOURCES = precomputation_test.cc
precomputation_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
suffix_array_test_SOURCES = suffix_array_test.cc
suffix_array_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
veb_test_SOURCES = veb_test.cc
veb_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a

noinst_LIBRARIES = libextractor.a libcompile.a

compile_SOURCES = compile.cc
compile_LDADD = libcompile.a
run_extractor_SOURCES = run_extractor.cc
run_extractor_LDADD = libextractor.a

libcompile_a_SOURCES = \
  alignment.cc \
  data_array.cc \
  phrase_location.cc \
  precomputation.cc \
  suffix_array.cc \
  translation_table.cc

libextractor_a_SOURCES = \
  alignment.cc \
  binary_search_merger.cc \
  data_array.cc \
  grammar_extractor.cc \
  matching.cc \
  matching_comparator.cc \
  matchings_finder.cc \
  intersector.cc \
  linear_merger.cc \
  matchings_trie.cc \
  phrase.cc \
  phrase_builder.cc \
  phrase_location.cc \
  precomputation.cc \
  rule_extractor.cc \
  rule_factory.cc \
  suffix_array.cc \
  translation_table.cc \
  veb.cc \
  veb_bitset.cc \
  veb_tree.cc \
  vocabulary.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -std=c++0x $(GTEST_CPPFLAGS) $(GMOCK_CPPFLAGS)
