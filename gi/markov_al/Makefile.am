bin_PROGRAMS = ml

ml_SOURCES = ml.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -funroll-loops -I$(top_srcdir)/utils $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder
AM_LDFLAGS = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz
