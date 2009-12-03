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

model1_SOURCES = model1.cc
model1_LDADD = libhg.a

grammar_convert_SOURCES = grammar_convert.cc

optimize_test_SOURCES = optimize_test.cc

collapse_weights_SOURCES = collapse_weights.cc

lbfgs_test_SOURCES = lbfgs_test.cc

mr_optimize_reduce_SOURCES = mr_optimize_reduce.cc
mr_optimize_reduce_LDADD = libhg.a

mr_em_train_SOURCES = mr_em_train.cc
mr_em_train_LDADD = libhg.a

plftools_SOURCES = plftools.cc
plftools_LDADD = libhg.a

LDADD = libhg.a

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LIB) -lz

noinst_LIBRARIES = libhg.a
