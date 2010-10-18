#noinst_PROGRAMS = \
#  ngram_test
#TESTS = ngram_test
#ngram_test_SOURCES = ngram_test.cc
#ngram_test_LDADD = ../util/libklm_util.a

noinst_LIBRARIES = libklm.a

libklm_a_SOURCES = \
  exception.cc \
  ngram.cc \
  ngram_build_binary.cc \
  ngram_query.cc \
  virtual_interface.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I..

