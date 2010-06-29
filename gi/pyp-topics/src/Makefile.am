bin_PROGRAMS = pyp-topics-train pyp-contexts-train

contexts_lexer.cc: contexts_lexer.l
	$(LEX) -s -CF -8 -o$@ $<

pyp_topics_train_SOURCES = corpus.cc gammadist.c gzstream.cc mt19937ar.c pyp-topics.cc train.cc contexts_lexer.cc contexts_corpus.cc
pyp_topics_train_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

pyp_contexts_train_SOURCES = corpus.cc gammadist.c gzstream.cc mt19937ar.c pyp-topics.cc contexts_lexer.cc contexts_corpus.cc train-contexts.cc
pyp_contexts_train_LDADD = $(top_srcdir)/decoder/libcdec.a -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -funroll-loops

