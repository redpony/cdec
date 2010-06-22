bin_PROGRAMS = \
  model1 \
  mr_em_map_adapter \
  mr_em_adapted_reduce \
  mr_reduce_to_weights \
  mr_optimize_reduce \
  grammar_convert \
  atools \
  plftools \
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

mr_em_map_adapter_SOURCES = mr_em_map_adapter.cc
mr_em_map_adapter_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

mr_reduce_to_weights_SOURCES = mr_reduce_to_weights.cc
mr_reduce_to_weights_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

mr_em_adapted_reduce_SOURCES = mr_em_adapted_reduce.cc
mr_em_adapted_reduce_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

plftools_SOURCES = plftools.cc
plftools_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder

