bin_PROGRAMS = cbgi brat dpnaive pfbrat pfdist itg pfnaive

noinst_LIBRARIES = libpf.a
libpf_a_SOURCES = base_measures.cc reachability.cc cfg_wfst_composer.cc corpus.cc

itg_SOURCES = itg.cc

dpnaive_SOURCES = dpnaive.cc

pfdist_SOURCES = pfdist.cc

pfnaive_SOURCES = pfnaive.cc

cbgi_SOURCES = cbgi.cc

brat_SOURCES = brat.cc

pfbrat_SOURCES = pfbrat.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -funroll-loops -I$(top_srcdir)/utils $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder
AM_LDFLAGS = libpf.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/utils/libutils.a -lz
