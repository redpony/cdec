
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
  bit_packing.hh \
  ersatz_progress.hh \
  exception.hh \
  file.hh \
  file_piece.hh \
  getopt.hh \
  have.hh \
  joint_sort.hh \
  mmap.hh \
  murmur_hash.hh \
  pcqueue.hh \
  pool.hh \
  probing_hash_table.hh \
  proxy_iterator.hh \
  read_compressed.hh \
  scoped.hh \
  sized_iterator.hh \
  sorted_uniform.hh \
  string_piece.hh \
  string_piece_hash.hh \
  thread_pool.hh \
  tokenize_piece.hh \
  usage.hh \
  ersatz_progress.cc \
  bit_packing.cc \
  exception.cc \
	file.cc \
  file_piece.cc \
  mmap.cc \
  murmur_hash.cc \
  pool.cc \
	read_compressed.cc \
  scoped.cc \
  string_piece.cc \
	usage.cc

AM_CPPFLAGS = -W -Wall -I$(top_srcdir)/klm -I$(top_srcdir)/klm/util/double-conversion
