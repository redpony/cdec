#ifndef UTOA_H
#define UTOA_H

#include <string>
#include <cstring>

#ifndef DIGIT_LOOKUP_TABLE
# define DIGIT_LOOKUP_TABLE 0
#endif

const unsigned utoa_bufsize=21;
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


inline char *itoa(char *p,int n) {
  if (n<0) {
    p=utoa(p,-n); // TODO: check that (unsigned)(-INT_MIN) == 0x1000000 in 2s complement and not == 0
    *--p='-';
    return p;
  } else
    return utoa(p,n);
}

inline std::string utos(unsigned n) {
  char buf[utoa_bufsize];
  char *end=buf+utoa_bufsize;
  char *p=utoa(end,n);
  return std::string(p,end);
}

inline std::string itos(int n) {
  char buf[utoa_bufsize];
  char *end=buf+utoa_bufsize;
  char *p=itoa(end,n);
  return std::string(p,end);
}

//returns position of '\0' terminating number written starting at to
inline char* append_utoa(char *to,unsigned n) {
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
inline char* append_itoa(char *to,unsigned n) {
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
