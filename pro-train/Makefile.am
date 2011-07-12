bin_PROGRAMS = \
  mr_pro_map \
  mr_pro_reduce

TESTS = lo_test

mr_pro_map_SOURCES = mr_pro_map.cc
mr_pro_map_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

mr_pro_reduce_SOURCES = mr_pro_reduce.cc
mr_pro_reduce_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/training/optimize.o $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval -I$(top_srcdir)/training
