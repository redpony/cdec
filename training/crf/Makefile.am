bin_PROGRAMS = \
  mpi_batch_optimize \
  mpi_compute_cllh \
  mpi_extract_features \
  mpi_extract_reachable \
  mpi_flex_optimize \
  mpi_online_optimize

mpi_online_optimize_SOURCES = mpi_online_optimize.cc
mpi_online_optimize_LDADD = $(top_srcdir)/training/utils/libtraining_utils.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/klm/search/libksearch.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a -lz

mpi_flex_optimize_SOURCES = mpi_flex_optimize.cc
mpi_flex_optimize_LDADD = $(top_srcdir)/training/utils/libtraining_utils.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/klm/search/libksearch.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a -lz

mpi_extract_reachable_SOURCES = mpi_extract_reachable.cc
mpi_extract_reachable_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/klm/search/libksearch.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a -lz

mpi_extract_features_SOURCES = mpi_extract_features.cc
mpi_extract_features_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/klm/search/libksearch.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a -lz

mpi_batch_optimize_SOURCES = mpi_batch_optimize.cc cllh_observer.cc
mpi_batch_optimize_LDADD = $(top_srcdir)/training/utils/libtraining_utils.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/klm/search/libksearch.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a -lz

mpi_compute_cllh_SOURCES = mpi_compute_cllh.cc cllh_observer.cc
mpi_compute_cllh_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/klm/search/libksearch.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a -lz

AM_CPPFLAGS = -DBOOST_TEST_DYN_LINK -W -Wall -Wno-sign-compare -I$(top_srcdir)/training -I$(top_srcdir)/training/utils -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval
