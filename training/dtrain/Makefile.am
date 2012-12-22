bin_PROGRAMS = dtrain

dtrain_SOURCES = dtrain.cc score.cc
dtrain_LDADD   = $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/klm/search/libksearch.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval

