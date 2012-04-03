bin_PROGRAMS = cbgi brat dpnaive pfbrat pfdist itg pfnaive condnaive align-lexonly-pyp learn_cfg pyp_lm nuisance_test align-tl pf_test bayes_lattice_score

noinst_LIBRARIES = libpf.a

libpf_a_SOURCES = base_distributions.cc reachability.cc cfg_wfst_composer.cc corpus.cc unigrams.cc ngram_base.cc transliterations.cc backward.cc hpyp_tm.cc pyp_tm.cc

bayes_lattice_score_SOURCES = bayes_lattice_score.cc
bayes_lattice_score_LDADD = libpf.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a -lz

pf_test_SOURCES = pf_test.cc
pf_test_LDADD = libpf.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a -lz

nuisance_test_SOURCES = nuisance_test.cc
nuisance_test_LDADD = libpf.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a -lz

align_lexonly_pyp_SOURCES = align-lexonly-pyp.cc
align_lexonly_pyp_LDADD = libpf.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a -lz

align_tl_SOURCES = align-tl.cc
align_tl_LDADD = libpf.a $(top_srcdir)/decoder/libcdec.a $(top_srcdir)/mteval/libmteval.a $(top_srcdir)/utils/libutils.a $(top_srcdir)/klm/lm/libklm.a $(top_srcdir)/klm/util/libklm_util.a -lz

itg_SOURCES = itg.cc

pyp_lm_SOURCES = pyp_lm.cc

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
