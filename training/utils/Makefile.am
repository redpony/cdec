noinst_LIBRARIES = libtraining_utils.a

bin_PROGRAMS = \
  sentserver \
  sentclient \
  grammar_convert

noinst_PROGRAMS = \
  lbfgs_test \
  optimize_test

EXTRA_DIST = decode-and-evaluate.pl libcall.pl parallelize.pl

sentserver_SOURCES = sentserver.cc
sentserver_LDFLAGS = -pthread

sentclient_SOURCES = sentclient.cc
sentclient_LDFLAGS = -pthread

TESTS = lbfgs_test optimize_test

libtraining_utils_a_SOURCES = \
  candidate_set.h \
  entropy.h \
  lbfgs.h \
  online_optimizer.h \
  optimize.h \
  risk.h \
  sentserver.h \
  candidate_set.cc \
  entropy.cc \
  optimize.cc \
  online_optimizer.cc \
  risk.cc

optimize_test_SOURCES = optimize_test.cc
optimize_test_LDADD = libtraining_utils.a ../../utils/libutils.a

grammar_convert_SOURCES = grammar_convert.cc
grammar_convert_LDADD = ../../decoder/libcdec.a ../../mteval/libmteval.a ../../utils/libutils.a

lbfgs_test_SOURCES = lbfgs_test.cc
lbfgs_test_LDADD = ../../utils/libutils.a

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -I$(top_srcdir)/decoder -I$(top_srcdir)/utils -I$(top_srcdir)/mteval -I$(top_srcdir)/klm

