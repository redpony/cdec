bin_PROGRAMS = cbgi brat dpnaive pfbrat pfdist itg pfnaive condnaive align-lexonly align-lexonly-pyp learn_cfg

noinst_LIBRARIES = libpf.a
libpf_a_SOURCES = base_distributions.cc reachability.cc cfg_wfst_composer.cc corpus.cc unigrams.cc ngram_base.cc

align_lexonly_SOURCES = align-lexonly.cc

align_lexonly_pyp_SOURCES = align-lexonly-pyp.cc

itg_SOURCES = itg.cc

learn_cfg_SOURCES = learn_cfg.cc

condnaive_SOURCES = condnaive.cc

dpnaive_SOURCES = dpnaive.cc

pfdist_SOURCES = pfdist.cc

pfnaive_SOURCES = pfnaive.cc

cbgi_SOURCES = cbgi.cc

brat_SOURCES = brat.cc

pfbrat_SOURCES = pfbrat.cc

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -funroll-loops -I$(top_srcdir)/utils $(GTEST_CPPFLAGS) -I$(top_srcdir)/decoder -I$(top_srcdir)/klm

AM_LDFLAGS = libpf.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a $(top_srcdir)/utils/libutils.a -lz
