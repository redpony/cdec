/* Optional packages.  You might want to integrate this with your build system e.g. config.h from ./configure. */
#ifndef UTIL_HAVE__
#define UTIL_HAVE__

#ifndef HAVE_ZLIB
#if !defined(_WIN32) && !defined(_WIN64)
#define HAVE_ZLIB
#endif
#endif

#ifndef HAVE_ICU
//#define HAVE_ICU
#endif

#ifndef HAVE_BOOST
#define HAVE_BOOST
#endif

#ifndef HAVE_THREADS
//#define HAVE_THREADS
#endif

#endif // UTIL_HAVE__
