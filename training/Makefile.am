bin_PROGRAMS = \
  model1 \
  lbl_model \
  test_ngram \
  mr_em_map_adapter \
  mr_em_adapted_reduce \
  mr_reduce_to_weights \
  mr_optimize_reduce \
  grammar_convert \
  plftools \
  collapse_weights \
  mpi_extract_reachable \
  mpi_extract_features \
  mpi_online_optimize \
  mpi_flex_optimize \
  mpi_batch_optimize \
  mpi_compute_cllh \
  augment_grammar

noinst_PROGRAMS = \
  lbfgs_test \
  optimize_test

TESTS = lbfgs_test optimize_test

noinst_LIBRARIES = libtraining.a
libtraining_a_SOURCES = \
  candidate_set.cc \
  entropy.cc \
  optimize.cc \
  online_optimizer.cc \
  risk.cc

mpi_online_optimize_SOURCES = mpi_online_optimize.cc
mpi_online_optimize_LDADD = libtraining.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

mpi_flex_optimize_SOURCES = mpi_flex_optimize.cc
mpi_flex_optimize_LDADD = libtraining.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

mpi_extract_reachable_SOURCES = mpi_extract_reachable.cc
mpi_extract_reachable_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

mpi_extract_features_SOURCES = mpi_extract_features.cc
mpi_extract_features_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

mpi_batch_optimize_SOURCES = mpi_batch_optimize.cc cllh_observer.cc
mpi_batch_optimize_LDADD = libtraining.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

mpi_compute_cllh_SOURCES = mpi_compute_cllh.cc cllh_observer.cc
mpi_compute_cllh_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

augment_grammar_SOURCES = augment_grammar.cc
augment_grammar_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

test_ngram_SOURCES = test_ngram.cc
test_ngram_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

model1_SOURCES = model1.cc ttables.cc
model1_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

lbl_model_SOURCES = lbl_model.cc
lbl_model_LDADD = libtraining.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

grammar_convert_SOURCES = grammar_convert.cc
grammar_convert_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

optimize_test_SOURCES = optimize_test.cc
optimize_test_LDADD = libtraining.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

collapse_weights_SOURCES = collapse_weights.cc
collapse_weights_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

lbfgs_test_SOURCES = lbfgs_test.cc
lbfgs_test_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

mr_optimize_reduce_SOURCES = mr_optimize_reduce.cc
mr_optimize_reduce_LDADD = libtraining.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

mr_em_map_adapter_SOURCES = mr_em_map_adapter.cc
mr_em_map_adapter_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

mr_reduce_to_weights_SOURCES = mr_reduce_to_weights.cc
mr_reduce_to_weights_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

mr_em_adapted_reduce_SOURCES = mr_em_adapted_reduce.cc
mr_em_adapted_reduce_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

plftools_SOURCES = plftools.cc
plftools_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder -I$(top_srcdir)/utils -I$(top_srcdir)/mteval -I../klm
