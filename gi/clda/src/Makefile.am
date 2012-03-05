bin_PROGRAMS = clda

clda_SOURCES = clda.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -funroll-loops -I$(top_srcdir)/utils $(GTEST_CPPFLAGS)
AM_LDFLAGS = $(top_srcdir)/utils/libutils.a -lz
