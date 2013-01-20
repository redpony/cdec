bin_PROGRAMS = minrisk_optimize

minrisk_optimize_SOURCES = minrisk_optimize.cc
minrisk_optimize_LDADD = ../../training/utils/libtraining_utils.a ../../decoder/libcdec.a ../../mteval/libmteval.a ../../utils/libutils.a ../../training/liblbfgs/liblbfgs.a

EXTRA_DIST = minrisk.pl minrisk_generate_input.pl

AM_CPPFLAGS = -W -Wall -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval -I$(top_srcdir)/training -I$(top_srcdir)/training/utils
