bin_PROGRAMS = gibbs_train_plm

gibbs_train_plm_SOURCES = gibbs_train_plm.cc
gibbs_train_plm_LDADD = $(top_srcdir)/utils/libutils.a -lz

AM_CPPFLAGS = -funroll-loops -W -Wall -Wno-sign-compare $(GTEST_CPPFLAGS) -I$(top_srcdir)/utils -I$(top_srcdir)/decoder -I$(top_srcdir)/mteval
