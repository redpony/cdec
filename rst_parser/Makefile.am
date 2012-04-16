bin_PROGRAMS = \
  mst_train rst_parse

noinst_PROGRAMS = \
  rst_test

TESTS = rst_test

noinst_LIBRARIES = librst.a

librst_a_SOURCES = arc_factored.cc arc_factored_marginals.cc rst.cc arc_ff.cc dep_training.cc

mst_train_SOURCES = mst_train.cc
mst_train_LDADD = librst.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a ../training/optimize.o -lz

rst_parse_SOURCES = rst_parse.cc
rst_parse_LDADD = librst.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

rst_test_SOURCES = rst_test.cc
rst_test_LDADD = librst.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder -I$(top_srcdir)/training -I$(top_srcdir)/utils -I$(top_srcdir)/mteval -I../klm
