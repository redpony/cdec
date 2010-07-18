bin_PROGRAMS = \
  extractor \
  mr_stripe_rule_reduce \
  build_lexical_translation \
  filter_grammar \
  featurize_grammar \
  filter_score_grammar \
  extractor_monolingual

noinst_PROGRAMS =

sg_lexer.cc: sg_lexer.l
	$(LEX) -s -CF -8 -o$@ $<

filter_score_grammar_SOURCES = filter_score_grammar.cc extract.cc sentence_pair.cc striped_grammar.cc
filter_score_grammar_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
filter_score_grammar_LDFLAGS = -all-static

filter_grammar_SOURCES = filter_grammar.cc extract.cc sentence_pair.cc striped_grammar.cc sg_lexer.cc
filter_grammar_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
filter_grammar_LDFLAGS = -all-static

featurize_grammar_SOURCES = featurize_grammar.cc extract.cc sentence_pair.cc sg_lexer.cc striped_grammar.cc
featurize_grammar_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
featurize_grammar_LDFLAGS = -all-static

build_lexical_translation_SOURCES = build_lexical_translation.cc extract.cc sentence_pair.cc
build_lexical_translation_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
build_lexical_translation_LDFLAGS = -all-static

mr_stripe_rule_reduce_SOURCES = mr_stripe_rule_reduce.cc extract.cc sentence_pair.cc striped_grammar.cc
mr_stripe_rule_reduce_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
mr_stripe_rule_reduce_LDFLAGS = -all-static

extractor_SOURCES = sentence_pair.cc extract.cc extractor.cc striped_grammar.cc
extractor_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
extractor_LDFLAGS = -all-static

extractor_monolingual_SOURCES = extractor_monolingual.cc
extractor_monolingual_LDADD = $(top_srcdir)/decoder/libcdec.a -lz
extractor_monolingual_LDFLAGS = -all-static

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder

