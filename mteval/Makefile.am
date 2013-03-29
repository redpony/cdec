bin_PROGRAMS = \
  fast_score \
  mbr_kbest

noinst_PROGRAMS = \
  scorer_test
TESTS = scorer_test

noinst_LIBRARIES = libmteval.a

EXTRA_DIST = test_data

libmteval_a_SOURCES = \
  aer_scorer.h \
  comb_scorer.h \
  external_scorer.h \
  ns.h \
  ns_cer.h \
  ns_comb.h \
  ns_docscorer.h \
  ns_ext.h \
  ns_ssk.h \
  ns_ter.h \
  scorer.h \
  ter.h \
  aer_scorer.cc \
  comb_scorer.cc \
  external_scorer.cc \
  meteor_jar.cc \
  ns.cc \
  ns_cer.cc \
  ns_comb.cc \
  ns_docscorer.cc \
  ns_ext.cc \
  ns_ssk.cc \
  ns_ter.cc \
  scorer.cc \
  ter.cc

fast_score_SOURCES = fast_score.cc
fast_score_LDADD = libmteval.a ../utils/libutils.a

mbr_kbest_SOURCES = mbr_kbest.cc
mbr_kbest_LDADD = libmteval.a ../utils/libutils.a

scorer_test_SOURCES = scorer_test.cc
scorer_test_LDADD = libmteval.a ../utils/libutils.a $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)

AM_CPPFLAGS = -DTEST_DATA=\"$(top_srcdir)/mteval/test_data\" -DBOOST_TEST_DYN_LINK -W -Wall -Wno-sign-compare -I$(top_srcdir) -I$(top_srcdir)/utils
