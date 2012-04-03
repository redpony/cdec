bin_PROGRAMS = \
  mst_train

noinst_PROGRAMS = \
  rst_test

TESTS = rst_test

noinst_LIBRARIES = librst.a

librst_a_SOURCES = rst.cc

mst_train_SOURCES = mst_train.cc
mst_train_LDADD = librst.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a ../klm/lm/libklm.a ../klm/util/libklm_util.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder -I$(top_srcdir)/utils -I$(top_srcdir)/mteval -I../klm
