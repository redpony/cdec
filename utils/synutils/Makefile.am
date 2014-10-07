bin_PROGRAMS = const_reorder_model_trainer

const_reorder_model_trainer_SOURCES = constituent_reorder_model.cc
const_reorder_model_trainer_LDADD = libfeature.a  maxent-3.0/libtsuruoka_maxent.a -lgfortran

noinst_LIBRARIES = libfeature.a

libfeature_a_SOURCES = \
  alignment.h \
  srl_sentence.h \
  tree.h \
  utility.h \
  argument_reorder_model.h \
  tsuruoka_maxent.h 
  
AM_CPPFLAGS = -W -Wall -I$(top_srcdir) -I$(top_srcdir)/utils -I$(top_srcdir)/synutils/maxent-3.0 
