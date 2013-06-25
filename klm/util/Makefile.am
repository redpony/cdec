
#noinst_PROGRAMS = \
#  file_piece_test \
#  joint_sort_test \
#  key_value_packing_test \
#  probing_hash_table_test \
#  sorted_uniform_test

#TESTS = \
#  file_piece_test \
#  joint_sort_test \
#  key_value_packing_test \
#  probing_hash_table_test \
#  sorted_uniform_test

#file_piece_test_SOURCES = file_piece_test.cc
#file_piece_test_LDADD = libklm_util.a

noinst_LIBRARIES = libklm_util.a

libklm_util_a_SOURCES = \
  bit_packing.cc \
  bit_packing.hh \
  ersatz_progress.cc \
  ersatz_progress.hh \
  exception.cc \
  exception.hh \
  fake_ofstream.hh \
  file.cc \
  file.hh \
  file_piece.cc \
  file_piece.hh \
  getopt.hh \
  have.hh \
  joint_sort.hh \
  mmap.cc \
  mmap.hh \
  multi_intersection.hh \
  murmur_hash.cc \
  murmur_hash.hh \
  pcqueue.hh \
  pool.cc \
  pool.hh \
  probing_hash_table.hh \
  proxy_iterator.hh \
  read_compressed.cc \
  read_compressed.hh \
  scoped.cc \
  scoped.hh \
  sized_iterator.hh \
  sorted_uniform.hh \
  string_piece.cc \
  string_piece.hh \
  string_piece_hash.hh \
  thread_pool.hh \
  tokenize_piece.hh \
  usage.cc \
  usage.hh

AM_CPPFLAGS = -W -Wall -I$(top_srcdir)/klm -I$(top_srcdir)/klm/util/double-conversion
