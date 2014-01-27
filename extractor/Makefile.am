
bin_PROGRAMS = sacompile run_extractor extract

EXTRA_PROGRAMS = alignment_test \
    data_array_test \
    fast_intersector_test \
    feature_count_source_target_test \
    feature_is_source_singleton_test \
    feature_is_source_target_singleton_test \
    feature_max_lex_source_given_target_test \
    feature_max_lex_target_given_source_test \
    feature_sample_source_count_test \
    feature_target_given_source_coherent_test \
    grammar_extractor_test \
    matchings_finder_test \
    matchings_sampler_test \
    phrase_location_sampler_test \
    phrase_test \
    precomputation_test \
    rule_extractor_helper_test \
    rule_extractor_test \
    rule_factory_test \
    scorer_test \
    suffix_array_sampler_test \
    suffix_array_test \
    target_phrase_extractor_test \
    translation_table_test \
    vocabulary_test

if HAVE_GTEST
  RUNNABLE_TESTS = alignment_test \
    data_array_test \
    fast_intersector_test \
    feature_count_source_target_test \
    feature_is_source_singleton_test \
    feature_is_source_target_singleton_test \
    feature_max_lex_source_given_target_test \
    feature_max_lex_target_given_source_test \
    feature_sample_source_count_test \
    feature_target_given_source_coherent_test \
    grammar_extractor_test \
    matchings_finder_test \
    matchings_sampler_test \
    phrase_location_sampler_test \
    phrase_test \
    precomputation_test \
    rule_extractor_helper_test \
    rule_extractor_test \
    rule_factory_test \
    scorer_test \
    suffix_array_sampler_test \
    suffix_array_test \
    target_phrase_extractor_test \
    translation_table_test \
    vocabulary_test
endif

noinst_PROGRAMS = $(RUNNABLE_TESTS)

TESTS = $(RUNNABLE_TESTS)

alignment_test_SOURCES = alignment_test.cc
alignment_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a
data_array_test_SOURCES = data_array_test.cc
data_array_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a
fast_intersector_test_SOURCES = fast_intersector_test.cc
fast_intersector_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
feature_count_source_target_test_SOURCES = features/count_source_target_test.cc
feature_count_source_target_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a
feature_is_source_singleton_test_SOURCES = features/is_source_singleton_test.cc
feature_is_source_singleton_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a
feature_is_source_target_singleton_test_SOURCES = features/is_source_target_singleton_test.cc
feature_is_source_target_singleton_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a
feature_max_lex_source_given_target_test_SOURCES = features/max_lex_source_given_target_test.cc
feature_max_lex_source_given_target_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
feature_max_lex_target_given_source_test_SOURCES = features/max_lex_target_given_source_test.cc
feature_max_lex_target_given_source_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
feature_sample_source_count_test_SOURCES = features/sample_source_count_test.cc
feature_sample_source_count_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a
feature_target_given_source_coherent_test_SOURCES = features/target_given_source_coherent_test.cc
feature_target_given_source_coherent_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a
grammar_extractor_test_SOURCES = grammar_extractor_test.cc
grammar_extractor_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
matchings_finder_test_SOURCES = matchings_finder_test.cc
matchings_finder_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
matchings_sampler_test_SOURCES = matchings_sampler_test.cc
matchings_sampler_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
phrase_location_sampler_test_SOURCES = phrase_location_sampler_test.cc
phrase_location_sampler_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
phrase_test_SOURCES = phrase_test.cc
phrase_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
precomputation_test_SOURCES = precomputation_test.cc
precomputation_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
rule_extractor_helper_test_SOURCES = rule_extractor_helper_test.cc
rule_extractor_helper_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
rule_extractor_test_SOURCES = rule_extractor_test.cc
rule_extractor_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
rule_factory_test_SOURCES = rule_factory_test.cc
rule_factory_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
scorer_test_SOURCES = scorer_test.cc
scorer_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
suffix_array_sampler_test_SOURCES = suffix_array_sampler_test.cc
suffix_array_sampler_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
suffix_array_test_SOURCES = suffix_array_test.cc
suffix_array_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
target_phrase_extractor_test_SOURCES = target_phrase_extractor_test.cc
target_phrase_extractor_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
translation_table_test_SOURCES = translation_table_test.cc
translation_table_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(GMOCK_LDFLAGS) $(GMOCK_LIBS) libextractor.a
vocabulary_test_SOURCES = vocabulary_test.cc
vocabulary_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) libextractor.a

noinst_LIBRARIES = libextractor.a

sacompile_SOURCES = sacompile.cc
sacompile_LDADD = libextractor.a
run_extractor_SOURCES = run_extractor.cc
run_extractor_LDADD = libextractor.a
extract_SOURCES = extract.cc
extract_LDADD = libextractor.a

libextractor_a_SOURCES = \
  alignment.cc \
  backoff_sampler.cc \
  data_array.cc \
  fast_intersector.cc \
  features/count_source_target.cc \
  features/feature.cc \
  features/is_source_singleton.cc \
  features/is_source_target_singleton.cc \
  features/max_lex_source_given_target.cc \
  features/max_lex_target_given_source.cc \
  features/sample_source_count.cc \
  features/target_given_source_coherent.cc \
  features/count_source_target.h \
  features/feature.h \
  features/is_source_singleton.h \
  features/is_source_target_singleton.h \
  features/max_lex_source_given_target.h \
  features/max_lex_target_given_source.h \
  features/sample_source_count.h \
  features/target_given_source_coherent.h \
  grammar.cc \
  grammar_extractor.cc \
  matchings_finder.cc \
  matchings_sampler.cc \
  matchings_trie.cc \
  phrase.cc \
  phrase_builder.cc \
  phrase_location.cc \
  phrase_location_sampler.cc \
  precomputation.cc \
  rule.cc \
  rule_extractor.cc \
  rule_extractor_helper.cc \
  rule_factory.cc \
  scorer.cc \
  suffix_array.cc \
  suffix_array_sampler.cc \
  target_phrase_extractor.cc \
  time_util.cc \
  translation_table.cc \
  vocabulary.cc \
  alignment.h \
  backoff_sampler.h \
  data_array.h \
  fast_intersector.h \
  grammar.h \
  grammar_extractor.h \
  matchings_finder.h \
  matchings_sampler.h \
  matchings_trie.h \
  phrase.h \
  phrase_builder.h \
  phrase_location.h \
  phrase_location_sampler.h \
  precomputation.h \
  rule.h \
  rule_extractor.h \
  rule_extractor_helper.h \
  rule_factory.h \
  sampler.h \
  scorer.h \
  suffix_array.h \
  suffix_array_sampler.h \
  target_phrase_extractor.h \
  time_util.h \
  translation_table.h \
  vocabulary.h

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(OPENMP_CXXFLAGS) $(GTEST_CPPFLAGS) $(GMOCK_CPPFLAGS)
AM_LDFLAGS = $(OPENMP_CXXFLAGS)
