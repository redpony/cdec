bin_PROGRAMS = \
  extractor \
  mr_stripe_rule_reduce \
  build_lexical_translation \
  filter_grammar \
  featurize_grammar \
  filter_score_grammar

noinst_PROGRAMS =

filter_score_grammar_SOURCES = filter_score_grammar.cc extract.cc sentence_pair.cc
filter_score_grammar_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
filter_score_grammar_LDFLAGS = -all-static

filter_grammar_SOURCES = filter_grammar.cc extract.cc sentence_pair.cc
filter_grammar_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
filter_grammar_LDFLAGS = -all-static

featurize_grammar_SOURCES = featurize_grammar.cc extract.cc sentence_pair.cc
featurize_grammar_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
featurize_grammar_LDFLAGS = -all-static

build_lexical_translation_SOURCES = build_lexical_translation.cc extract.cc sentence_pair.cc
build_lexical_translation_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
build_lexical_translation_LDFLAGS = -all-static

mr_stripe_rule_reduce_SOURCES = mr_stripe_rule_reduce.cc extract.cc sentence_pair.cc
mr_stripe_rule_reduce_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
mr_stripe_rule_reduce_LDFLAGS = -all-static

extractor_SOURCES = sentence_pair.cc extract.cc extractor.cc
extractor_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
extractor_LDFLAGS = -all-static

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder

