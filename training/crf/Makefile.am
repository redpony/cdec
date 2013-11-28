bin_PROGRAMS = \
  mpi_batch_optimize \
  mpi_adagrad_optimize \
  mpi_compute_cllh \
  mpi_extract_features \
  mpi_extract_reachable \
  mpi_flex_optimize \
  mpi_online_optimize \
  mpi_baum_welch

mpi_baum_welch_SOURCES = mpi_baum_welch.cc
mpi_baum_welch_LDADD = ../../decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a -lz

mpi_adagrad_optimize_SOURCES = mpi_adagrad_optimize.cc cllh_observer.cc cllh_observer.h
mpi_adagrad_optimize_LDADD = ../../training/utils/libtraining_utils.a ../../decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a -lz

mpi_online_optimize_SOURCES = mpi_online_optimize.cc
mpi_online_optimize_LDADD = ../../training/utils/libtraining_utils.a ../../decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a -lz

mpi_flex_optimize_SOURCES = mpi_flex_optimize.cc
mpi_flex_optimize_LDADD = ../../training/utils/libtraining_utils.a ../../decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a -lz

mpi_extract_reachable_SOURCES = mpi_extract_reachable.cc
mpi_extract_reachable_LDADD = ../../decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a -lz

mpi_extract_features_SOURCES = mpi_extract_features.cc
mpi_extract_features_LDADD = ../../decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a -lz

mpi_batch_optimize_SOURCES = mpi_batch_optimize.cc cllh_observer.cc cllh_observer.h
mpi_batch_optimize_LDADD = ../../training/utils/libtraining_utils.a ../../decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a -lz

mpi_compute_cllh_SOURCES = mpi_compute_cllh.cc cllh_observer.cc cllh_observer.h
mpi_compute_cllh_LDADD = ../../decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a -lz

AM_CPPFLAGS = -DBOOST_TEST_DYN_LINK -W -Wall -Wno-sign-compare -I$(top_srcdir)/training -I$(top_srcdir)/training/utils -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval
