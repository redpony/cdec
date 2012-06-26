bin_PROGRAMS = minrisk_optimize

minrisk_optimize_SOURCES = minrisk_optimize.cc
minrisk_optimize_LDADD = $(top_srcdir)/training/libtraining.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/training/liblbfgs/liblbfgs.a -lz

AM_CPPFLAGS = -W -Wall $(GTEST_CPPFLAGS) -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval -I$(top_srcdir)/training
