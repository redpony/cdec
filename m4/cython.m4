dnl Taken from the python bindings to the Enlightenment foundation libraries, 
dnl and was part of a GPL package. I have included this file to fix the build.
dnl
dnl
dnl AM_CHECK_CYTHON([VERSION [,ACTION-IF-FOUND [,ACTION-IF-NOT-FOUND]]])
dnl Check if a Cython version is installed
dnl Defines CYTHON_VERSION and CYTHON_FOUND
AC_DEFUN([AM_CHECK_CYTHON],
[
AC_REQUIRE([AM_PATH_PYTHON])
ifelse([$1], [], [_msg=""], [_msg=" >= $1"])
AC_MSG_CHECKING(for Cython$_msg)
AC_CACHE_VAL(py_cv_cython, [

prog="import Cython.Compiler.Version; print Cython.Compiler.Version.version"
CYTHON_VERSION=`$PYTHON -c "$prog" 2>&AC_FD_CC`

py_cv_cython=no
if test "x$CYTHON_VERSION" != "x"; then
   py_cv_cython=yes
fi

if test "x$py_cv_cython" = "xyes"; then
   ifelse([$1], [], [:],
      AS_VERSION_COMPARE([$CYTHON_VERSION], [$1], [py_cv_cython=no]))
fi
])

AC_MSG_RESULT([$py_cv_cython])

if test "x$py_cv_cython" = "xyes"; then
   CYTHON_FOUND=yes
   ifelse([$2], [], [:], [$2])
else
   CYTHON_FOUND=no
   ifelse([$3], [], [AC_MSG_ERROR([Could not find usable Cython$_msg])], [$3])
fi
])

dnl AM_CHECK_CYTHON_PRECOMPILED(FILE-LIST [, ACTION-IF-ALL [, ACTION-IF-NOT-ALL]])
dnl given a list of files ending in .pyx (FILE-LIST), check if their .c
dnl counterpart exists and is not older than the source.
dnl    ACTION-IF-ALL is called only if no files failed the check and thus
dnl       all pre-generated files are usable.
dnl    ACTION-IF-NOT-ALL is called if some or all failed. If not provided,
dnl       an error will be issued.
AC_DEFUN([AM_CHECK_CYTHON_PRECOMPILED],
[
_to_check_list="$1"
_failed_list=""
_exists_list=""

for inf in $_to_check_list; do
    outf=`echo "$inf" | sed -e 's/^\(.*\)[.]pyx$/\1.c/'`
    if test "$outf" = "$inf"; then
       AC_MSG_WARN([File to check must end in .pyx, but got: $inf -- Skip])
       continue
    fi

    AC_MSG_CHECKING([for pre-generated $outf for $inf])
    if ! test -f "$outf"; then
       _res=no
       _failed_list="${_failed_list} $outf"
    elif ! test "$outf" -nt "$inf"; then
       _res="no (older)"
       _failed_list="${_failed_list} $outf"
    else
       _res=yes
       _exists_list="${_exists_list} $outf"
    fi
    AC_MSG_RESULT($_res)
done

if test -z "$_failed_list" -a -n "$_exists_list"; then
   ifelse([$2], [], [:], [$2])
else
   ifelse([$3], [],
   [AC_MSG_ERROR([Missing pre-generated files: $_failed_list])],
   [$3])
fi
])
