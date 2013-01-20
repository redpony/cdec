bin_PROGRAMS = fast_align

fast_align_SOURCES = fast_align.cc ttables.cc da.h ttables.h
fast_align_LDADD = ../utils/libutils.a

EXTRA_DIST = aligner.pl ortho-norm support makefiles stemmers

AM_CPPFLAGS = -W -Wall -I$(top_srcdir) -I$(top_srcdir)/utils -I$(top_srcdir)/training
