bin_PROGRAMS = \
  mr_pro_map \
  mr_pro_reduce

mr_pro_map_SOURCES = mr_pro_map.cc
mr_pro_map_LDADD = ../../training/utils/libtraining_utils.a ../../decoder/libcdec.a ../../mteval/libmteval.a ../../utils/libutils.a

mr_pro_reduce_SOURCES = mr_pro_reduce.cc
mr_pro_reduce_LDADD = ../../training/liblbfgs/liblbfgs.a ../../utils/libutils.a

EXTRA_DIST = mr_pro_generate_mapper_input.pl pro.pl

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval -I$(top_srcdir)/training/utils -I$(top_srcdir)/training
