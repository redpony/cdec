bin_PROGRAMS = latent_svm

latent_svm_SOURCES = latent_svm.cc
latent_svm_LDADD = ../..//decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval
