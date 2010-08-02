#ifndef STATIC_UTOA_H
#define STATIC_UTOA_H

#include "threadlocal.h"
#include <cstring>

#define DIGIT_LOOKUP_TABLE 0

namespace {
THREADLOCAL char utoa_buf[] = "01234567890123456789"; // to put end of string character at buf[20]
const unsigned utoa_bufsize=sizeof(utoa_buf);
const unsigned utoa_bufsizem1=utoa_bufsize-1;
#ifdef DIGIT_LOOKUP_TABLE
char digits[] = "0123456789";
#endif
}

inline char digit_to_char(int d) {
  return
#ifdef DIGIT_LOOKUP_TABLE
    digits[d];
#else
    '0'+d;
#endif
}


// returns n in string [return,num); *num=0 yourself calling if you want a c_str
inline char *utoa(char *num,unsigned n) {
  if ( !n ) {
    *--num='0';
  } else {
    unsigned rem;
    // 3digit lookup table, divide by 1000 faster?
    while ( n ) {
#if 1
      rem = n;
      n /= 10;
      rem -= 10*n;		// maybe this is faster than mod because we are already dividing
#else
      rem = n%10; // would optimizer combine these together?
      n   = n/10;
#endif
      *--num = digit_to_char(rem);
    }
  }
  return num;
}

inline char *static_utoa(unsigned n) {
  return utoa(utoa_buf+utoa_bufsizem1,n);
}

//returns position of '\0' terminating number written starting at to
inline char* append_utoa(char *to,unsigned n) {
  char *s=static_utoa(n);
  int ns=(utoa_buf+utoa_bufsize)-s;
  std::memcpy(to,s,ns);
  return to+ns;
}


#endif
