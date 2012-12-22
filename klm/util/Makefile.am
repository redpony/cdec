
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
  ersatz_progress.cc \
  bit_packing.cc \
  exception.cc \
	file.cc \
  file_piece.cc \
  mmap.cc \
  murmur_hash.cc \
  pool.cc \
	read_compressed.cc \
  string_piece.cc \
	usage.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I..
