bin_PROGRAMS = dtrain

dtrain_SOURCES = dtrain.cc score.cc dtrain.h kbestget.h ksampler.h pairsampling.h score.h
dtrain_LDADD   = ../../decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a -lboost_regex

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval

