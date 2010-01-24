bin_PROGRAMS = \
  mr_vest_map \
  mr_vest_reduce \
  mr_vest_generate_mapper_input \
  fast_score \
  union_forests

if HAVE_GTEST
noinst_PROGRAMS = \
  scorer_test \
  lo_test
endif

union_forests_SOURCES = union_forests.cc
union_forests_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

fast_score_SOURCES = fast_score.cc ter.cc comb_scorer.cc scorer.cc viterbi_envelope.cc
fast_score_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

mr_vest_generate_mapper_input_SOURCES = mr_vest_generate_mapper_input.cc line_optimizer.cc
mr_vest_generate_mapper_input_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

mr_vest_map_SOURCES = viterbi_envelope.cc error_surface.cc mr_vest_map.cc scorer.cc ter.cc comb_scorer.cc line_optimizer.cc
mr_vest_map_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

mr_vest_reduce_SOURCES = error_surface.cc mr_vest_reduce.cc scorer.cc ter.cc comb_scorer.cc line_optimizer.cc viterbi_envelope.cc
mr_vest_reduce_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

scorer_test_SOURCES = scorer_test.cc scorer.cc ter.cc comb_scorer.cc viterbi_envelope.cc
scorer_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(top_srcdir)/decoder/libcdec.a -lz

lo_test_SOURCES = lo_test.cc scorer.cc ter.cc comb_scorer.cc viterbi_envelope.cc error_surface.cc line_optimizer.cc
lo_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(top_srcdir)/decoder/libcdec.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(BOOST_CPPFLAGS) $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder
AM_LDFLAGS = $(BOOST_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LIB)
