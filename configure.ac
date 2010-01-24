AC_INIT
AM_INIT_AUTOMAKE(cdec,0.1)
AC_CONFIG_HEADERS(config.h)
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_CXX
AC_LANG_CPLUSPLUS
BOOST_REQUIRE
BOOST_PROGRAM_OPTIONS
CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
LIBS="$LIBS $BOOST_PROGRAM_OPTIONS_LIBS"

AC_CHECK_HEADER(boost/math/special_functions/digamma.hpp,
               [AC_DEFINE([HAVE_BOOST_DIGAMMA], [], [flag for boost::math::digamma])])

AC_PROG_INSTALL
GTEST_LIB_CHECK

AC_ARG_WITH(srilm,
            [AC_HELP_STRING([--with-srilm=PATH], [(optional) path to SRI's LM toolkit])],
            [with_srilm=$withval],
            [with_srilm=no]
           )

AM_CONDITIONAL([SRI_LM], false)

if test "x$with_srilm" != 'xno'
then
  SAVE_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS -I${with_srilm}/include"

  AC_CHECK_HEADER(Ngram.h,
                 [AC_DEFINE([HAVE_SRILM], [], [flag for SRILM])],
                 [AC_MSG_ERROR([Cannot find SRILM!])])

  LIB_SRILM="-loolm -ldstruct -lmisc"
  # ROOT/lib/i686-m64/liboolm.a
  # ROOT/lib/i686-m64/libdstruct.a
  # ROOT/lib/i686-m64/libmisc.a
  MY_ARCH=`${with_srilm}/sbin/machine-type`
  LDFLAGS="$LDFLAGS -L${with_srilm}/lib/${MY_ARCH}"
  LIBS="$LIBS $LIB_SRILM"
  FMTLIBS="$FMTLIBS liboolm.a libdstruct.a libmisc.a"
  AM_CONDITIONAL([SRI_LM], true)
fi

AC_OUTPUT(Makefile decoder/Makefile training/Makefile vest/Makefile)

