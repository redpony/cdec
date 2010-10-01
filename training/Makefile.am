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
  cllh_filter_grammar \
  online_train

noinst_PROGRAMS = \
  lbfgs_test \
  optimize_test

TESTS = lbfgs_test optimize_test


if MPI
bin_PROGRAMS += mpi_batch_optimize \
             mpi_online_optimize \
             compute_cllh

mpi_batch_optimize_SOURCES = mpi_batch_optimize.cc optimize.cc
mpi_batch_optimize_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

mpi_online_optimize_SOURCES = mpi_online_optimize.cc online_optimizer.cc
mpi_online_optimize_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

compute_cllh_SOURCES = compute_cllh.cc
compute_cllh_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz
endif

cllh_filter_grammar_SOURCES = cllh_filter_grammar.cc
cllh_filter_grammar_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

online_train_SOURCES = online_train.cc online_optimizer.cc
online_train_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

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

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder -I$(top_srcdir)/utils -I$(top_srcdir)/mteval
