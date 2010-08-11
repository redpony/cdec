bin_PROGRAMS = \
  mr_vest_map \
  mr_vest_reduce \
  mr_vest_generate_mapper_input \
  sentserver \
  sentclient

if HAVE_GTEST
noinst_PROGRAMS = \
  lo_test
endif

sentserver_SOURCES = sentserver.c

sentclient_SOURCES = sentclient.c

mr_vest_generate_mapper_input_SOURCES = mr_vest_generate_mapper_input.cc line_optimizer.cc
mr_vest_generate_mapper_input_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

mr_vest_map_SOURCES = viterbi_envelope.cc ces.cc error_surface.cc mr_vest_map.cc line_optimizer.cc
mr_vest_map_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

mr_vest_reduce_SOURCES = error_surface.cc ces.cc mr_vest_reduce.cc line_optimizer.cc viterbi_envelope.cc
mr_vest_reduce_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

lo_test_SOURCES = lo_test.cc ces.cc viterbi_envelope.cc error_surface.cc line_optimizer.cc
lo_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval
