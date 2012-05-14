bin_PROGRAMS = \
  creg

creg_SOURCES = creg.cc json_feature_map_lexer.cc
creg_LDADD = $(top_srcdir)/training/liblbfgs/liblbfgs.a $(top_srcdir)/utils/libutils.a -lz

json_feature_map_lexer.cc: json_feature_map_lexer.ll
	$(LEX) -s -8 -CF -o$@ $<

AM_CPPFLAGS = -W -Wall -DNDEBUG -I$(top_srcdir)/utils -I$(top_srcdir)/training

