# TODO I'm sure I can leave something out.
bin_PROGRAMS = dtrain dtest

dtrain_SOURCES = dtrain.cc dcommon.cc
dtrain_LDADD   = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

dtest_SOURCES = dtest.cc dcommon.cc
dtest_LDADD   = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval

