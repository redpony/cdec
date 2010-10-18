if HAVE_GTEST
noinst_PROGRAMS = \
  ngram_test
TESTS = ngram_test
endif

noinst_LIBRARIES = libklm.a

libklm_a_SOURCES = \
  exception.cc \
  ngram.cc \
  ngram_build_binary.cc \
  ngram_query.cc \
  virtual_interface.cc

ngram_test_SOURCES = ngram_test.cc
ngram_test_LDADD = ../util/libklm_util.a

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I..

