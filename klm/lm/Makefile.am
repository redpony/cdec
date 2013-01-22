bin_PROGRAMS = build_binary ngram_query

build_binary_SOURCES = build_binary_main.cc
build_binary_LDADD = libklm.a ../util/libklm_util.a ../util/double-conversion/libklm_util_double.a -lz

ngram_query_SOURCES = query_main.cc
ngram_query_LDADD = libklm.a ../util/libklm_util.a ../util/double-conversion/libklm_util_double.a -lz

#noinst_PROGRAMS = \
#  ngram_test
#TESTS = ngram_test
#ngram_test_SOURCES = ngram_test.cc
#ngram_test_LDADD = ../util/libklm_util.a

noinst_LIBRARIES = libklm.a

libklm_a_SOURCES = \
  bhiksha.hh \
  binary_format.hh \
  blank.hh \
  config.hh \
  enumerate_vocab.hh \
  facade.hh \
  left.hh \
  lm_exception.hh \
  max_order.hh \
  model.hh \
  model_type.hh \
  ngram_query.hh \
  partial.hh \
  quantize.hh \
  read_arpa.hh \
  return.hh \
  search_hashed.hh \
  search_trie.hh \
  sizes.hh \
  state.hh \
  trie.hh \
  trie_sort.hh \
  value.hh \
  value_build.hh \
  virtual_interface.hh \
  vocab.hh \
  weights.hh \
  word_index.hh \
  bhiksha.cc \
  binary_format.cc \
  config.cc \
  lm_exception.cc \
	quantize.cc \
  model.cc \
  read_arpa.cc \
  search_hashed.cc \
  search_trie.cc \
  sizes.cc \
  trie.cc \
	trie_sort.cc \
	value_build.cc \
  virtual_interface.cc \
  vocab.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -I$(top_srcdir)/klm

