noinst_LIBRARIES = liblbfgs.a

noinst_PROGRAMS = ll_test
ll_test_SOURCES = ll_test.cc
ll_test_LDADD = liblbfgs.a -lz
TESTS = ll_test

liblbfgs_a_SOURCES = \
  lbfgs.c \
  arithmetic_ansi.h \
  arithmetic_sse_double.h \
  arithmetic_sse_float.h \
  lbfgs++.h \
  lbfgs.h

################################################################
# do NOT NOT NOT add any other -I includes NO NO NO NO NO ######
AM_LDFLAGS = liblbfgs.a -lz
AM_CPPFLAGS = -DBOOST_TEST_DYN_LINK -W -Wall -I$(top_srcdir)/training -I$(top_srcdir)/training/liblbfgs
################################################################

