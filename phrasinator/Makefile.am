bin_PROGRAMS = gibbs_train_plm gibbs_train_plm_notables

#head_bigram_model

gibbs_train_plm_notables_SOURCES = gibbs_train_plm.notables.cc
gibbs_train_plm_notables_LDADD = $(top_srcdir)/utils/libutils.a -lz

gibbs_train_plm_SOURCES = gibbs_train_plm.cc
gibbs_train_plm_LDADD = $(top_srcdir)/utils/libutils.a -lz

#head_bigram_model_SOURCES = head_bigram_model.cc
#head_bigram_model_LDADD = $(top_srcdir)/utils/libutils.a -lz

AM_CPPFLAGS = -funroll-loops -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval
