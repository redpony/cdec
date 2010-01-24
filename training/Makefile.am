bin_PROGRAMS = \
  model1 \
  mr_optimize_reduce \
  grammar_convert \
  atools \
  plftools \
  mr_em_train \
  collapse_weights

noinst_PROGRAMS = \
  lbfgs_test \
  optimize_test

atools_SOURCES = atools.cc
atools_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

model1_SOURCES = model1.cc
model1_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

grammar_convert_SOURCES = grammar_convert.cc
grammar_convert_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

optimize_test_SOURCES = optimize_test.cc optimize.cc
optimize_test_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

collapse_weights_SOURCES = collapse_weights.cc
collapse_weights_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

lbfgs_test_SOURCES = lbfgs_test.cc
lbfgs_test_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

mr_optimize_reduce_SOURCES = mr_optimize_reduce.cc optimize.cc
mr_optimize_reduce_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

mr_em_train_SOURCES = mr_em_train.cc
mr_em_train_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

plftools_SOURCES = plftools.cc
plftools_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(BOOST_CPPFLAGS) $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder
AM_LDFLAGS = $(BOOST_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LIB)

