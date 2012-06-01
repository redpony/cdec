TESTS = ll_test
noinst_PROGRAMS = ll_test

ll_test_SOURCES = ll_test.cc
ll_test_LDADD = liblbfgs.a -lz

noinst_LIBRARIES = liblbfgs.a

liblbfgs_a_SOURCES = lbfgs.c

################################################################
# do NOT NOT NOT add any other -I includes NO NO NO NO NO ######
AM_LDFLAGS = liblbfgs.a -lz
AM_CPPFLAGS = -DBOOST_TEST_DYN_LINK -W -Wall -I. -I..
################################################################
