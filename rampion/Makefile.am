bin_PROGRAMS = rampion_cccp

rampion_cccp_SOURCES = rampion_cccp.cc
rampion_cccp_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval -I$(top_srcdir)/training
