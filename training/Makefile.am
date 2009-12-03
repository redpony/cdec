bin_PROGRAMS = \
  model1 \
  mr_optimize_reduce \
  grammar_convert \
  atools \
  plftools \
  lbfgs_test \
  mr_em_train \
  collapse_weights \
  optimize_test

atools_SOURCES = atools.cc
atools_LDADD = $(top_srcdir)/src/libhg.a -lz

model1_SOURCES = model1.cc
model1_LDADD = $(top_srcdir)/src/libhg.a -lz

grammar_convert_SOURCES = grammar_convert.cc
grammar_convert_LDADD = $(top_srcdir)/src/libhg.a -lz

optimize_test_SOURCES = optimize_test.cc optimize.cc
optimize_test_LDADD = $(top_srcdir)/src/libhg.a -lz

collapse_weights_SOURCES = collapse_weights.cc
collapse_weights_LDADD = $(top_srcdir)/src/libhg.a -lz

lbfgs_test_SOURCES = lbfgs_test.cc
lbfgs_test_LDADD = $(top_srcdir)/src/libhg.a -lz

mr_optimize_reduce_SOURCES = mr_optimize_reduce.cc optimize.cc
mr_optimize_reduce_LDADD = $(top_srcdir)/src/libhg.a -lz

mr_em_train_SOURCES = mr_em_train.cc
mr_em_train_LDADD = $(top_srcdir)/src/libhg.a -lz

plftools_SOURCES = plftools.cc
plftools_LDADD = $(top_srcdir)/src/libhg.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(BOOST_CPPFLAGS) $(GTEST_CPPFLAGS) -I$(top_srcdir)/src
AM_LDFLAGS = $(BOOST_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LIB)

