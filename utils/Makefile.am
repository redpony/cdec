
bin_PROGRAMS = reconstruct_weights

noinst_PROGRAMS = ts phmt
TESTS = ts phmt

if HAVE_GTEST
noinst_PROGRAMS += \
  dict_test \
  weights_test \
  logval_test \
  small_vector_test

TESTS += small_vector_test logval_test weights_test dict_test
endif

reconstruct_weights_SOURCES = reconstruct_weights.cc

noinst_LIBRARIES = libutils.a

libutils_a_SOURCES = \
  alignment_pharaoh.cc \
  b64tools.cc \
  dict.cc \
  tdict.cc \
  fdict.cc \
  gzstream.cc \
  filelib.cc \
  stringlib.cc \
  sparse_vector.cc \
  timing_stats.cc \
  verbose.cc \
  weights.cc

if HAVE_CMPH
  libutils_a_SOURCES += perfect_hash.cc
endif

phmt_SOURCES = phmt.cc
ts_SOURCES = ts.cc
dict_test_SOURCES = dict_test.cc
dict_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)
weights_test_SOURCES = weights_test.cc
weights_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)
logval_test_SOURCES = logval_test.cc
logval_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)
small_vector_test_SOURCES = small_vector_test.cc
small_vector_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)

AM_LDFLAGS = libutils.a -lz

################################################################
# do NOT NOT NOT add any other -I includes NO NO NO NO NO ######
AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I.
################################################################
