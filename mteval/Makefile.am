bin_PROGRAMS = \
  fast_score \
  mbr_kbest

if HAVE_GTEST
noinst_PROGRAMS = \
  scorer_test
endif

noinst_LIBRARIES = libmteval.a

libmteval_a_SOURCES = ter.cc comb_scorer.cc aer_scorer.cc scorer.cc

fast_score_SOURCES = fast_score.cc
fast_score_LDADD = $(top_srcdir)/utils/libutils.a libmteval.a -lz

mbr_kbest_SOURCES = mbr_kbest.cc
mbr_kbest_LDADD = $(top_srcdir)/utils/libutils.a libmteval.a -lz

scorer_test_SOURCES = scorer_test.cc
scorer_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(top_srcdir)/utils/libutils.a libmteval.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/utils
