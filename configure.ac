AC_CONFIG_MACRO_DIR([m4])
AC_INIT([cdec],[2014-01-28])
AC_CONFIG_SRCDIR([decoder/cdec.cc])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(config.h)
AC_PROG_LIBTOOL
AC_PROG_LEX
case $LEX in 
:) AC_MSG_ERROR([No lex (Flex, lex, etc.) program found]);; 
esac 
OLD_CXXFLAGS=$CXXFLAGS
AC_PROG_CC
AC_PROG_CXX
CXXFLAGS=$OLD_CXXFLAGS
AX_CXX_COMPILE_STDCXX_11([],[mandatory])
AC_LANG_CPLUSPLUS
AC_OPENMP
BOOST_REQUIRE([1.44])
BOOST_FILESYSTEM
BOOST_PROGRAM_OPTIONS
BOOST_SYSTEM
BOOST_SERIALIZATION
BOOST_TEST
BOOST_THREADS
AM_PATH_PYTHON
AC_CHECK_HEADER(dlfcn.h,AC_DEFINE(HAVE_DLFCN_H))
AC_CHECK_LIB(rt, clock_gettime)
AC_CHECK_LIB(dl, dlopen)
AC_CHECK_HEADERS(zlib.h,
    AC_CHECK_LIB(z, gzread,[
        AC_DEFINE(HAVE_ZLIB,[],[Do we have zlib])
        ZLIBS="$ZLIBS -lz"
    ]))

AC_CHECK_HEADERS(bzlib.h,
    AC_CHECK_LIB(bz2, BZ2_bzReadOpen,[
        AC_DEFINE(HAVE_BZLIB,[],[Do we have bzlib])
        ZLIBS="$ZLIBS -lbz2"
    ]))

AC_CHECK_HEADERS(lzma.h,
    AC_CHECK_LIB(lzma, lzma_code,[
        AC_DEFINE(HAVE_XZLIB,[],[Do we have lzma])
        ZLIBS="$ZLIBS -llzma"
    ]))

AC_ARG_ENABLE(mpi,
 [ --enable-mpi  Build MPI binaries, assumes mpi.h is present ],
 [ mpi=yes
 ])
AM_CONDITIONAL([MPI], [test "x$mpi" = xyes])

if test "x$mpi" = xyes
then
  AC_DEFINE([HAVE_MPI], [1], [flag for MPI])
  LIBS="$LIBS -lboost_mpi"
fi

AM_CONDITIONAL([HAVE_METEOR], false)
AC_ARG_WITH(meteor,
            [AC_HELP_STRING([--with-meteor=PATH], [(optional) path to METEOR jar])],
            [with_meteor=$withval],
            [with_meteor=no]
           )

if test "x$with_meteor" != 'xno'
then
  AC_CHECK_FILE([$with_meteor],
                [AC_DEFINE([HAVE_METEOR], [1], [flag for METEOR jar library])],
                [AC_MSG_ERROR([Cannot find METEOR jar!])])
  AC_SUBST(METEOR_JAR,"${with_meteor}")
  AM_CONDITIONAL([HAVE_METEOR], true)
fi

AM_CONDITIONAL([HAVE_CMPH], false)
AC_ARG_WITH(cmph,
            [AC_HELP_STRING([--with-cmph=PATH], [(optional) path to cmph perfect hashing library])],
            [with_cmph=$withval],
            [with_cmph=no]
           )

if test "x$with_cmph" != 'xno'
then
  SAVE_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS -I${with_cmph}/include"

  AC_CHECK_HEADER(cmph.h,
                 [AC_DEFINE([HAVE_CMPH], [1], [flag for cmph perfect hashing library])],
                 [AC_MSG_ERROR([Cannot find cmph library!])])

  LDFLAGS="$LDFLAGS -L${with_cmph}/lib"
  AC_CHECK_LIB(cmph, cmph_search)
  AM_CONDITIONAL([HAVE_CMPH], true)
fi

AM_CONDITIONAL([HAVE_GTEST], false)
AC_ARG_WITH(gtest,
            [AC_HELP_STRING([--with-gtest=DIR], [(optional) path to Google Test library])],
            [with_gtest=$withval],
            [with_gtest=no]
           )

AM_CONDITIONAL([HAVE_GMOCK], false)
AC_ARG_WITH(gmock,
            [AC_HELP_STRING([--with-gmock=DIR], [(optional) path to Google Mock library])],
            [with_gmock=$withval],
            [with_gmock=no]
           )

if test "x$with_gtest" != 'xno'
then
  gtest_CPPFLAGS="-I${with_gtest}/include"
  gtest_LDFLAGS="-L${with_gtest} -L${with_gtest}/lib"
  gtest_LIBS="-lgtest_main -lgtest -lpthread"

  SAVECPP_FLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $gtest_CPPFLAGS"
  AC_CHECK_HEADER(${with_gtest}/include/gtest/gtest.h,
                  [AC_DEFINE([HAVE_GTEST], [1], [flag for Google Test header])],
                  [AC_MSG_ERROR([Cannot find Google Test headers!])]
                 )

  SAVE_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS $gtest_LDFLAGS"
  SAVE_LIBS="$LIBS"
  # Google Test needs pthreads.
  AC_CHECK_LIB([pthread],
               [pthread_mutex_init],
               [],
               [AC_MSG_ERROR([Cannot find pthread library])]
              )
  AX_CXX_CHECK_LIB([gtest],
                   [testing::TestInfo::name() const],
                   [],
                   [AC_MSG_ERROR([Cannot find Google Test library libgtest])]
                  )
  AC_CHECK_LIB([gtest_main],
               [main],
               [],
               [AC_MSG_ERROR([Cannot find Google Test library libgtest_main])]
              )

  AC_SUBST(AS_TR_CPP([GTEST_CPPFLAGS]), ["$gtest_CPPFLAGS"])
  AC_SUBST(AS_TR_CPP([GTEST_LDFLAGS]), ["$gtest_LDFLAGS"])
  AC_SUBST(AS_TR_CPP([GTEST_LIBS]), ["$gtest_LIBS"])


  if test "x$with_gmock" != 'xno'
  then
    gmock_CPPFLAGS="-I${with_gmock}/include"
    gmock_LDFLAGS="-L${with_gmock} -L${with_gmock}/lib"
    gmock_LIBS="-lgmock"

    CPPFLAGS="$CPPFLAGS $gmock_CPPFLAGS"
    AC_CHECK_HEADER(${with_gmock}/include/gmock/gmock.h,
                    [AC_DEFINE([HAVE_GMOCK], [1], [flag for Google Mock header])],
                    [AC_MSG_ERROR([Cannot find Google Mock headers!])]
                   )

    LDFLAGS="$LDFLAGS $gmock_LDFLAGS"
    AX_CXX_CHECK_LIB([gmock],
                     [testing::Expectation],
                     [],
                     [AC_MSG_ERROR([Cannot find Google Mock library libgmock])]
                    )

    AC_SUBST(AS_TR_CPP([GMOCK_CPPFLAGS]), ["$gmock_CPPFLAGS"])
    AC_SUBST(AS_TR_CPP([GMOCK_LDFLAGS]), ["$gmock_LDFLAGS"])
    AC_SUBST(AS_TR_CPP([GMOCK_LIBS]), ["$gmock_LIBS"])
    AM_CONDITIONAL([HAVE_GMOCK], true)
  fi

  CPPFLAGS="$SAVE_CPPFLAGS"
  LDFLAGS="$SAVE_LDFLAGS"
  LIBS="$SAVE_LIBS"
  AM_CONDITIONAL([HAVE_GTEST], true)
fi

#BOOST_THREADS
CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
LDFLAGS="$LDFLAGS $BOOST_PROGRAM_OPTIONS_LDFLAGS $BOOST_SERIALIZATION_LDFLAGS $BOOST_SYSTEM_LDFLAGS $BOOST_FILESYSTEM_LDFLAGS"
# $BOOST_THREAD_LDFLAGS"
LIBS="$LIBS $BOOST_PROGRAM_OPTIONS_LIBS $BOOST_SERIALIZATION_LIBS $BOOST_SYSTEM_LIBS $BOOST_FILESYSTEM_LIBS $ZLIBS"
# $BOOST_THREAD_LIBS"

AC_CHECK_HEADER(google/dense_hash_map,
               [AC_DEFINE([HAVE_SPARSEHASH], [1], [flag for google::dense_hash_map])])

AC_PROG_INSTALL

CPPFLAGS="-DPIC $CPPFLAGS -DHAVE_CONFIG_H -DKENLM_MAX_ORDER=6"
CXXFLAGS="$CXX11_SWITCH $CXXFLAGS -fPIC -g -O3"
CFLAGS="$CFLAGS -fPIC -g -O3"

if test "x$HAVE_CXX11" = "x0"; then
  CPPFLAGS="$CPPFLAGS -DHAVE_OLD_CPP"
fi

# core cdec stuff
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([utils/Makefile])
AC_CONFIG_FILES([mteval/Makefile])
AC_CONFIG_FILES([mteval/meteor_jar.cc])
AC_CONFIG_FILES([decoder/Makefile])
AC_CONFIG_FILES([python/setup.py])
AC_CONFIG_FILES([extractor/Makefile])
AC_CONFIG_FILES([word-aligner/Makefile])

# KenLM stuff
AC_CONFIG_FILES([klm/util/double-conversion/Makefile])
AC_CONFIG_FILES([klm/util/stream/Makefile])
AC_CONFIG_FILES([klm/util/Makefile])
AC_CONFIG_FILES([klm/lm/Makefile])
AC_CONFIG_FILES([klm/search/Makefile])
AC_CONFIG_FILES([klm/lm/builder/Makefile])

# training stuff
AC_CONFIG_FILES([training/Makefile])
AC_CONFIG_FILES([training/utils/Makefile])
AC_CONFIG_FILES([training/liblbfgs/Makefile])
AC_CONFIG_FILES([training/crf/Makefile])
AC_CONFIG_FILES([training/dpmert/Makefile])
AC_CONFIG_FILES([training/pro/Makefile])
AC_CONFIG_FILES([training/rampion/Makefile])
AC_CONFIG_FILES([training/minrisk/Makefile])
AC_CONFIG_FILES([training/mira/Makefile])
AC_CONFIG_FILES([training/latent_svm/Makefile])
AC_CONFIG_FILES([training/dtrain/Makefile])

# external feature function example code
AC_CONFIG_FILES([example_extff/Makefile])

AC_OUTPUT

