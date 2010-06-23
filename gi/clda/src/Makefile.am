bin_PROGRAMS = clda

clda_SOURCES = clda.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -funroll-loops -I$(top_srcdir)/decoder
AM_LDFLAGS = $(top_srcdir)/decoder/libcdec.a -lz
