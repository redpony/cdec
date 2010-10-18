if HAVE_GTEST
noinst_PROGRAMS = \
  scorer_test
TESTS = scorer_test
endif

noinst_LIBRARIES = libklm_util.a

libklm_util_a_SOURCES = \
  ersatz_progress.cc \
  exception.cc \
  file_piece.cc \
  mmap.cc \
  murmur_hash.cc \
  scoped.cc \
  string_piece.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I..
