bin_PROGRAMS = kbest_mira \
		kbest_cut_mira 

EXTRA_DIST = mira.py

kbest_mira_SOURCES = kbest_mira.cc
kbest_mira_LDFLAGS= -rdynamic
kbest_mira_LDADD = ../../decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a


kbest_cut_mira_SOURCES = kbest_cut_mira.cc
kbest_cut_mira_LDFLAGS= -rdynamic
kbest_cut_mira_LDADD = ../../decoder/libcdec.a ../../klm/search/libksearch.a ../../mteval/libmteval.a ../../utils/libutils.a ../../klm/lm/libklm.a ../../klm/util/libklm_util.a ../../klm/util/double-conversion/libklm_util_double.a

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval
