bin_PROGRAMS = \
  mr_dpmert_map \
  mr_dpmert_reduce \
  mr_dpmert_generate_mapper_input \
  sentserver \
  sentclient

if HAVE_GTEST
noinst_PROGRAMS = \
  lo_test
TESTS = lo_test
endif

sentserver_SOURCES = sentserver.c
sentserver_LDFLAGS = -all-static -pthread

sentclient_SOURCES = sentclient.c
sentclient_LDFLAGS = -all-static -pthread

mr_dpmert_generate_mapper_input_SOURCES = mr_dpmert_generate_mapper_input.cc line_optimizer.cc
mr_dpmert_generate_mapper_input_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

# nbest2hg_SOURCES = nbest2hg.cc
# nbest2hg_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lfst -lz

mr_dpmert_map_SOURCES = mert_geometry.cc ces.cc error_surface.cc mr_dpmert_map.cc line_optimizer.cc
mr_dpmert_map_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

mr_dpmert_reduce_SOURCES = error_surface.cc ces.cc mr_dpmert_reduce.cc line_optimizer.cc mert_geometry.cc
mr_dpmert_reduce_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

lo_test_SOURCES = lo_test.cc ces.cc mert_geometry.cc error_surface.cc line_optimizer.cc
lo_test_LDADD = $(GTEST_LDFLAGS) $(GTEST_LIBS) $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval
