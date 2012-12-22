noinst_LIBRARIES = libtraining_utils.a

bin_PROGRAMS = \
  sentserver \
  sentclient \
  grammar_convert

noinst_PROGRAMS = \
  lbfgs_test \
  optimize_test

sentserver_SOURCES = sentserver.c
sentserver_LDFLAGS = -pthread

sentclient_SOURCES = sentclient.c
sentclient_LDFLAGS = -pthread

TESTS = lbfgs_test optimize_test

libtraining_utils_a_SOURCES = \
  candidate_set.cc \
  entropy.cc \
  optimize.cc \
  online_optimizer.cc \
  risk.cc

optimize_test_SOURCES = optimize_test.cc
optimize_test_LDADD = libtraining_utils.a $(top_srcdir)/utils/libutils.a

grammar_convert_SOURCES = grammar_convert.cc
grammar_convert_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a

lbfgs_test_SOURCES = lbfgs_test.cc
lbfgs_test_LDADD = $(top_srcdir)/utils/libutils.a

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -I$(top_srcdir)/decoder -I$(top_srcdir)/utils -I$(top_srcdir)/mteval -I$(top_srcdir)/klm

