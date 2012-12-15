bin_PROGRAMS = reconstruct_weights atools

noinst_PROGRAMS = \
  ts \
  phmt \
  dict_test \
  m_test \
  weights_test \
  logval_test \
  small_vector_test

TESTS = ts small_vector_test logval_test weights_test dict_test m_test

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

reconstruct_weights_SOURCES = reconstruct_weights.cc
reconstruct_weights_LDADD = libutils.a
atools_SOURCES = atools.cc
atools_LDADD = libutils.a

phmt_SOURCES = phmt.cc
phmt_LDADD = libutils.a $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)
ts_SOURCES = ts.cc
ts_LDADD = libutils.a $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)
m_test_SOURCES = m_test.cc
m_test_LDADD = libutils.a $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)
dict_test_SOURCES = dict_test.cc
dict_test_LDADD = libutils.a $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)
weights_test_SOURCES = weights_test.cc
weights_test_LDADD = libutils.a $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)
logval_test_SOURCES = logval_test.cc
logval_test_LDADD = libutils.a $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)
small_vector_test_SOURCES = small_vector_test.cc
small_vector_test_LDADD = libutils.a $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)

################################################################
# do NOT NOT NOT add any other -I includes NO NO NO NO NO ######
AM_CPPFLAGS = -DBOOST_TEST_DYN_LINK -W -Wall -I.
################################################################
