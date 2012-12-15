bin_PROGRAMS = \
  mr_pro_map \
  mr_pro_reduce

mr_pro_map_SOURCES = mr_pro_map.cc
mr_pro_map_LDADD = $(top_srcdir)/training/utils/libtraining_utils.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a

mr_pro_reduce_SOURCES = mr_pro_reduce.cc
mr_pro_reduce_LDADD = $(top_srcdir)/training/liblbfgs/liblbfgs.a $(top_srcdir)/utils/libutils.a

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval -I$(top_srcdir)/training/utils -I$(top_srcdir)/training
