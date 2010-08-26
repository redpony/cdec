if HAVE_GTEST
noinst_PROGRAMS = \
  crp_test

TESTS = crp_test

crp_test_SOURCES = crp_test.cc
crp_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)

endif

bin_PROGRAMS = clda

clda_SOURCES = clda.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -funroll-loops -I$(top_srcdir)/utils $(GTEST_CPPFLAGS)
AM_LDFLAGS = $(top_srcdir)/utils/libutils.a -lz
