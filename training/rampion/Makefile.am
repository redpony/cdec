bin_PROGRAMS = rampion_cccp

rampion_cccp_SOURCES = rampion_cccp.cc
rampion_cccp_LDADD = ../../training/utils/libtraining_utils.a ../../decoder/libcdec.a ../../mteval/libmteval.a ../../utils/libutils.a

EXTRA_DIST = rampion.pl rampion_generate_input.pl

AM_CPPFLAGS = -W -Wall $(GTEST_CPPFLAGS) -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval -I$(top_srcdir)/training/utils
