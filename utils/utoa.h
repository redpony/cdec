#ifndef UTOA_H
#define UTOA_H

#include <stdint.h>
#include <string>
#include <cstring>

#ifndef DIGIT_LOOKUP_TABLE
# define DIGIT_LOOKUP_TABLE 0
#endif

// The largest 32-bit integer is 4294967295, that is 10 chars
// 1 more for sign, and 1 for 0-termination of string
// generally: 2 + std::numeric_limits<T>::is_signed + std::numeric_limits<T>::digits10
const unsigned utoa_bufsize=12;
const unsigned utoa_bufsizem1=utoa_bufsize-1;

#ifdef DIGIT_LOOKUP_TABLE
namespace {
char digits[] = "0123456789";
}
#endif

inline char digit_to_char(int d) {
  return
#ifdef DIGIT_LOOKUP_TABLE
    digits[d];
#else
    '0'+d;
#endif
}

// returns n in string [return,num); *num=0 yourself before calling if you want a c_str
inline char *utoa(char *num,uint32_t n) {
  if ( !n ) {
    *--num='0';
  } else {
    uint32_t rem;
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

inline char *itoa(char *p,int32_t n) {
  if (n<0) {
    p=utoa(p,-n); // (unsigned)(-INT_MIN) == 0x1000000 in 2s complement and not == 0.
    *--p='-';
    return p;
  } else
    return utoa(p,n);
}

inline std::string utos(uint32_t n) {
  char buf[utoa_bufsize];
  char *end=buf+utoa_bufsize;
  char *p=utoa(end,n);
  return std::string(p,end);
}

inline std::string itos(int32_t n) {
  char buf[utoa_bufsize];
  char *end=buf+utoa_bufsize;
  char *p=itoa(end,n);
  return std::string(p,end);
}

//returns position of '\0' terminating number written starting at to
inline char* append_utoa(char *to,uint32_t n) {
  char buf[utoa_bufsize];
  char *end=buf+utoa_bufsize;
  char *s=utoa(end,n);
  int ns=end-s;
  std::memcpy(to,s,ns);
  to+=ns;
  *to++=0;
  return to;
}

//returns position of '\0' terminating number written starting at to
inline char* append_itoa(char *to,int32_t n) {
  char buf[utoa_bufsize];
  char *end=buf+utoa_bufsize;
  char *s=itoa(end,n);
  int ns=end-s;
  std::memcpy(to,s,ns);
  to+=ns;
  *to++=0;
  return to;
}

#endif
