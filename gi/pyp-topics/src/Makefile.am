bin_PROGRAMS = pyp-topics-train pyp-contexts-train #mpi-pyp-contexts-train

contexts_lexer.cc: contexts_lexer.l
	$(LEX) -s -CF -8 -o$@ $<

pyp_topics_train_SOURCES = mt19937ar.c corpus.cc gzstream.cc pyp-topics.cc train.cc contexts_lexer.cc contexts_corpus.cc
pyp_topics_train_LDADD = $(top_srcdir)/utils/libutils.a -lz

pyp_contexts_train_SOURCES = mt19937ar.c corpus.cc gzstream.cc pyp-topics.cc contexts_lexer.cc contexts_corpus.cc train-contexts.cc
pyp_contexts_train_LDADD = $(top_srcdir)/utils/libutils.a -lz

#mpi_pyp_contexts_train_SOURCES = mt19937ar.c corpus.cc gzstream.cc mpi-pyp-topics.cc contexts_lexer.cc contexts_corpus.cc mpi-train-contexts.cc
#mpi_pyp_contexts_train_LDADD = $(top_srcdir)/utils/libutils.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -funroll-loops -I../../../utils

