bin_PROGRAMS = latent_svm

latent_svm_SOURCES = latent_svm.cc
latent_svm_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval
