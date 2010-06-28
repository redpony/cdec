bin_PROGRAMS = pyp-topics-train

contexts_lexer.cc: contexts_lexer.l
	$(LEX) -s -CF -8 -o$@ $<

pyp_topics_train_SOURCES = corpus.cc gammadist.c gzstream.cc mt19937ar.c pyp-topics.cc train.cc contexts_lexer.cc contexts_corpus.cc
pyp_topics_train_LDADD = -lz

AM_CPPFLAGS = -W -Wall -Wno-sign-compare -funroll-loops

