bin_PROGRAMS = \
  model1 \
  test_ngram \
  mr_em_map_adapter \
  mr_em_adapted_reduce \
  mr_reduce_to_weights \
  mr_optimize_reduce \
  grammar_convert \
  atools \
  plftools \
  collapse_weights \
  cllh_filter_grammar \
  mpi_online_optimize \
  mpi_batch_optimize \
  mpi_em_optimize \
  compute_cllh \
  feature_expectations \
  augment_grammar

noinst_PROGRAMS = \
  lbfgs_test \
  optimize_test

TESTS = lbfgs_test optimize_test

mpi_online_optimize_SOURCES = mpi_online_optimize.cc online_optimizer.cc
mpi_online_optimize_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

mpi_batch_optimize_SOURCES = mpi_batch_optimize.cc optimize.cc
mpi_batch_optimize_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

feature_expectations_SOURCES = feature_expectations.cc
feature_expectations_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

mpi_em_optimize_SOURCES = mpi_em_optimize.cc optimize.cc
mpi_em_optimize_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

compute_cllh_SOURCES = compute_cllh.cc
compute_cllh_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

cllh_filter_grammar_SOURCES = cllh_filter_grammar.cc
cllh_filter_grammar_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

augment_grammar_SOURCES = augment_grammar.cc
augment_grammar_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

test_ngram_SOURCES = test_ngram.cc
test_ngram_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

atools_SOURCES = atools.cc
atools_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

model1_SOURCES = model1.cc ttables.cc
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

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder -I$(top_srcdir)/utils -I$(top_srcdir)/mteval -I../klm
