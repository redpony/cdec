bin_PROGRAMS = reconstruct_weights atools

noinst_PROGRAMS = \
  ts \
  phmt \
  dict_test \
  m_test \
  weights_test \
  logval_test \
  small_vector_test \
  sv_test

TESTS = ts small_vector_test logval_test weights_test dict_test m_test sv_test

noinst_LIBRARIES = libutils.a

# EXTRA_DIST = test_data

libutils_a_SOURCES = \
  test_data \
  alias_sampler.h \
  alignment_io.h \
  array2d.h \
  b64tools.h \
  batched_append.h \
  city.h \
  citycrc.h \
  corpus_tools.h \
  dict.h \
  fast_sparse_vector.h \
  fdict.h \
  feature_vector.h \
  filelib.h \
  gzstream.h \
  hash.h \
  have_64_bits.h \
  indices_after.h \
  kernel_string_subseq.h \
  logval.h \
  m.h \
  murmur_hash.h \
  named_enum.h \
  null_deleter.h \
  null_traits.h \
  perfect_hash.h \
  prob.h \
  sampler.h \
  semiring.h \
  show.h \
  small_vector.h \
  sparse_vector.h \
  static_utoa.h \
  stringlib.h \
  tdict.h \
  timing_stats.h \
  utoa.h \
  value_array.h \
  verbose.h \
  warning_compiler.h \
  warning_pop.h \
  warning_push.h \
  weights.h \
  wordid.h \
  writer.h \
  fast_lexical_cast.hpp \
  intrusive_refcount.hpp \
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
sv_test_SOURCES = sv_test.cc
sv_test_LDADD = libutils.a $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)

################################################################
# do NOT NOT NOT add any other -I includes NO NO NO NO NO ######
AM_CPPFLAGS = -DBOOST_TEST_DYN_LINK -W -Wall -I. -I$(top_srcdir) -DTEST_DATA=\"$(top_srcdir)/utils/test_data\"
################################################################

