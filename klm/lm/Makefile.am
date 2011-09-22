bin_PROGRAMS = build_binary

build_binary_SOURCES = build_binary.cc
build_binary_LDADD = libklm.a ../util/libklm_util.a -lz

#noinst_PROGRAMS = \
#  ngram_test
#TESTS = ngram_test
#ngram_test_SOURCES = ngram_test.cc
#ngram_test_LDADD = ../util/libklm_util.a

noinst_LIBRARIES = libklm.a

libklm_a_SOURCES = \
  bhiksha.cc \
  binary_format.cc \
  config.cc \
  lm_exception.cc \
	quantize.cc \
  model.cc \
  ngram_query.cc \
  read_arpa.cc \
  search_hashed.cc \
  search_trie.cc \
  trie.cc \
	trie_sort.cc \
  virtual_interface.cc \
  vocab.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I..

