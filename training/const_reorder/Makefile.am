noinst_LIBRARIES = libtrainer.a

libtrainer_a_SOURCES = trainer.h trainer.cc

bin_PROGRAMS = const_reorder_model_trainer argument_reorder_model_trainer

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/utils -I$(top_srcdir)/decoder

const_reorder_model_trainer_SOURCES = constituent_reorder_model.cc
const_reorder_model_trainer_LDADD = ../../utils/libutils.a libtrainer.a
argument_reorder_model_trainer_SOURCES = argument_reorder_model.cc
argument_reorder_model_trainer_LDADD = ../../utils/libutils.a libtrainer.a
