bin_PROGRAMS = lmplz

lmplz_SOURCES = \
  lmplz_main.cc \
  adjust_counts.cc \
  adjust_counts.hh \
  corpus_count.cc \
  corpus_count.hh \
  discount.hh \
  header_info.hh \
  initial_probabilities.cc \
  initial_probabilities.hh \
  interpolate.cc \
  interpolate.hh \
  joint_order.hh \
  multi_stream.hh \
  ngram.hh \
  ngram_stream.hh \
  pipeline.cc \
  pipeline.hh \
  print.cc \
  print.hh \
  sort.hh

lmplz_LDADD = ../libklm.a ../../util/double-conversion/libklm_util_double.a ../../util/stream/libklm_util_stream.a ../../util/libklm_util.a $(BOOST_THREAD_LIBS)

AM_CPPFLAGS = -W -Wall -I$(top_srcdir)/klm

