bin_PROGRAMS = fast_align

fast_align_SOURCES = fast_align.cc ttables.cc
fast_align_LDADD = $(top_srcdir)/utils/libutils.a

AM_CPPFLAGS = -W -Wall $(GTEST_CPPFLAGS) -I$(top_srcdir)/utils -I$(top_srcdir)/training
