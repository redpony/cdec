#ifndef STATIC_UTOA_H
#define STATIC_UTOA_H

#include "threadlocal.h"


#include <string>
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

// so named to avoid gcc segfault when named itoa
inline char *itoa(char *p,int n) {
  if (n<0) {
    p=utoa(p,-n); // TODO: check that (unsigned)(-INT_MIN) == 0x1000000 in 2s complement and not == 0
    *--p='-';
    return p;
  } else
    return utoa(p,n);
}

inline char *static_itoa(int n) {
  return itoa(utoa_buf+utoa_bufsizem1,n);
}


inline std::string utos(unsigned n) {
  const int bufsz=20;
  char buf[bufsz];
  char *end=buf+bufsz;
  char *p=utoa(end,n);
  return std::string(p,end);
}

inline std::string itos(int n) {
  const int bufsz=20;
  char buf[bufsz];
  char *end=buf+bufsz;
  char *p=itoa(end,n);
  return std::string(p,end);
}

#ifdef ITOA_SAMPLE
# include <cstdio>
# include <sstream>
# include <iostream>
using namespace std;

int main(int argc,char *argv[]) {
  printf("d U d U d U\n");
  for (int i=1;i<argc;++i) {
    int n;
    unsigned un;
    sscanf(argv[i],"%d",&n);
    sscanf(argv[i],"%u",&un);
    printf("%d %u %s",n,un,static_itoa(n));
    printf(" %s %s %s\n",static_utoa(un),itos(n).c_str(),utos(un).c_str());
  }
  return 0;
}
#endif

#endif
