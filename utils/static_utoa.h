#ifndef STATIC_UTOA_H
#define STATIC_UTOA_H

#include "threadlocal.h"
#include "utoa.h"

namespace {
static const int utoa_bufsize=40; // 64bit safe.
static const int utoa_bufsizem1=utoa_bufsize-1; // 64bit safe.
static char utoa_buf[utoa_bufsize]; // to put end of string character at buf[20]
}

inline char *static_utoa(unsigned n) {
  assert(utoa_buf[utoa_bufsizem1]==0);
  return utoa(utoa_buf+utoa_bufsizem1,n);
}

inline char *static_itoa(int n) {
  assert(utoa_buf[utoa_bufsizem1]==0);
  return itoa(utoa_buf+utoa_bufsizem1,n);
}

#endif
