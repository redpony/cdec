bin_PROGRAMS = \
  fast_score \
  mbr_kbest

noinst_PROGRAMS = \
  scorer_test
TESTS = scorer_test

noinst_LIBRARIES = libmteval.a

libmteval_a_SOURCES = \
  aer_scorer.cc \
  comb_scorer.cc \
  external_scorer.cc \
  meteor_jar.cc \
  ns.cc \
  ns_cer.cc \
  ns_comb.cc \
  ns_docscorer.cc \
  ns_ext.cc \
  ns_ter.cc \
  scorer.cc \
  ter.cc

fast_score_SOURCES = fast_score.cc
fast_score_LDADD = libmteval.a $(top_srcdir)/utils/libutils.a

mbr_kbest_SOURCES = mbr_kbest.cc
mbr_kbest_LDADD = libmteval.a $(top_srcdir)/utils/libutils.a

scorer_test_SOURCES = scorer_test.cc
scorer_test_LDADD = libmteval.a $(top_srcdir)/utils/libutils.a $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)

AM_CPPFLAGS = -DBOOST_TEST_DYN_LINK -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/utils
