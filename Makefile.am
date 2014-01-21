# warning - the subdirectories in the following list should
# be kept in topologically sorted order. Also, DO NOT introduce
# cyclic dependencies between these directories!
SUBDIRS = \
  utils \
  mteval \
  klm/util/double-conversion \
  klm/util \
  klm/util/stream \
  klm/lm \
  klm/lm/builder \
  klm/search \
  decoder \
  training \
  word-aligner \
  extractor \
  example_extff


EXTRA_DIST = corpus tests python/cdec python/tests python/examples compound-split environment
AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -D_GLIBCXX_PARALLEL -march=native -mtune=native -O2 -pipe -fomit-frame-pointer -Wall

