bin_PROGRAMS = \
  mr_dpmert_map \
  mr_dpmert_reduce \
  mr_dpmert_generate_mapper_input

noinst_PROGRAMS = \
  lo_test
TESTS = lo_test

mr_dpmert_generate_mapper_input_SOURCES = mr_dpmert_generate_mapper_input.cc line_optimizer.cc
mr_dpmert_generate_mapper_input_LDADD = ../../decoder/libcdec.a ../../mteval/libmteval.a ../../utils/libutils.a

# nbest2hg_SOURCES = nbest2hg.cc
# nbest2hg_LDADD = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a -lfst

mr_dpmert_map_SOURCES = mert_geometry.cc ces.cc error_surface.cc mr_dpmert_map.cc line_optimizer.cc ces.h error_surface.h line_optimizer.h mert_geometry.h
mr_dpmert_map_LDADD = ../../decoder/libcdec.a ../../mteval/libmteval.a ../../utils/libutils.a

mr_dpmert_reduce_SOURCES = error_surface.cc ces.cc mr_dpmert_reduce.cc line_optimizer.cc mert_geometry.cc ces.h error_surface.h line_optimizer.h mert_geometry.h
mr_dpmert_reduce_LDADD = ../../decoder/libcdec.a ../../mteval/libmteval.a ../../utils/libutils.a

lo_test_SOURCES = lo_test.cc ces.cc mert_geometry.cc error_surface.cc line_optimizer.cc ces.h error_surface.h line_optimizer.h mert_geometry.h
lo_test_LDADD = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) ../../decoder/libcdec.a ../../mteval/libmteval.a ../../utils/libutils.a

EXTRA_DIST = test_data dpmert.pl

AM_CPPFLAGS = -DTEST_DATA=\"$(top_srcdir)/training/dpmert/test_data\" -DBOOST_TEST_DYN_LINK -W -Wall -Wno-sign-compare -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval
