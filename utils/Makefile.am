
bin_PROGRAMS = reconstruct_weights atools

noinst_PROGRAMS = ts phmt mfcr_test
TESTS = ts phmt mfcr_test

if HAVE_GTEST
noinst_PROGRAMS += \
  crp_test \
  dict_test \
  m_test \
  weights_test \
  logval_test \
  small_vector_test

TESTS += crp_test small_vector_test logval_test weights_test dict_test m_test
endif

reconstruct_weights_SOURCES = reconstruct_weights.cc

atools_SOURCES = atools.cc

noinst_LIBRARIES = libutils.a

libutils_a_SOURCES = \
  alignment_io.cc \
  b64tools.cc \
  corpus_tools.cc \
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
m_test_SOURCES = m_test.cc
m_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)
dict_test_SOURCES = dict_test.cc
dict_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)
mfcr_test_SOURCES = mfcr_test.cc
mfcr_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)
weights_test_SOURCES = weights_test.cc
weights_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)
crp_test_SOURCES = crp_test.cc
crp_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)
logval_test_SOURCES = logval_test.cc
logval_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)
small_vector_test_SOURCES = small_vector_test.cc
small_vector_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS)

AM_LDFLAGS = libutils.a -lz

################################################################
# do NOT NOT NOT add any other -I includes NO NO NO NO NO ######
AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I.
################################################################
