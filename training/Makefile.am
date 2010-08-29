bin_PROGRAMS = \
  model1 \
  mr_em_map_adapter \
  mr_em_adapted_reduce \
  mr_reduce_to_weights \
  mr_optimize_reduce \
  grammar_convert \
  atools \
  plftools \
  collapse_weights \
  online_train

noinst_PROGRAMS = \
  lbfgs_test \
  optimize_test

online_train_SOURCES = online_train.cc online_optimizer.cc
online_train_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

atools_SOURCES = atools.cc
atools_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

model1_SOURCES = model1.cc
model1_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

grammar_convert_SOURCES = grammar_convert.cc
grammar_convert_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

optimize_test_SOURCES = optimize_test.cc optimize.cc online_optimizer.cc
optimize_test_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

collapse_weights_SOURCES = collapse_weights.cc
collapse_weights_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

lbfgs_test_SOURCES = lbfgs_test.cc
lbfgs_test_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

mr_optimize_reduce_SOURCES = mr_optimize_reduce.cc optimize.cc
mr_optimize_reduce_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

mr_em_map_adapter_SOURCES = mr_em_map_adapter.cc
mr_em_map_adapter_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

mr_reduce_to_weights_SOURCES = mr_reduce_to_weights.cc
mr_reduce_to_weights_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

mr_em_adapted_reduce_SOURCES = mr_em_adapted_reduce.cc
mr_em_adapted_reduce_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

plftools_SOURCES = plftools.cc
plftools_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder -I$(top_srcdir)/utils -I$(top_srcdir)/mteval
