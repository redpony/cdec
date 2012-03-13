#ifndef UTOA_H
#define UTOA_H


#include <stdint.h>
#include <string>
#include <cstring>
#include <limits>

// define this if you're paranoid about converting 0-9 (int) to 0-9 (char) by adding to '0', which is safe for ascii, utf8, etc.
#ifndef DIGIT_LOOKUP_TABLE
# define DIGIT_LOOKUP_TABLE 0
#endif

//TODO: 3 decimal digit lookup table, divide by 1000 faster?
//TODO: benchmark these two (also, some assembly that does effectively divmod?)
#if 1
// maybe this is faster than mod because we are already dividing
#define NDIV10MOD(rem,n) rem = n; n /= 10; rem -= 10*n;
#else
// or maybe optimizer does it just as well or better with this:
#define NDIV10MOD(rem,n) rem = n%10; n = n/10;
#endif

template <class T>
struct signed_for_int {
};


#define DEFINE_SIGNED_FOR_3(t,it,ut)          \
template <> \
struct signed_for_int<t> { \
  typedef ut unsigned_t; \
  typedef it signed_t; \
  typedef t original_t; \
  enum { toa_bufsize = 3 + std::numeric_limits<t>::digits10, toa_bufsize_minus_1=toa_bufsize-1 }; \
};

// toa_bufsize will hold enough chars for a c string converting to sign,digits (for both signed and unsigned types), because normally an unsigned would only need 2 extra chars.  we reserve 3 explicitly for the case that itoa(buf,UINT_MAX,true) is called, with output +4......

#define DEFINE_SIGNED_FOR(it) DEFINE_SIGNED_FOR_3(it,it,u ## it)    \
  DEFINE_SIGNED_FOR_3(u ## it,it,u ## it)

DEFINE_SIGNED_FOR(int8_t)
DEFINE_SIGNED_FOR(int16_t)
DEFINE_SIGNED_FOR(int32_t)
DEFINE_SIGNED_FOR(int64_t)
//DEFINE_SIGNED_FOR_3(int,int,unsigned)
//DEFINE_SIGNED_FOR_3(unsigned,int,unsigned)

/*
// The largest 32-bit integer is 4294967295, that is 10 chars
// 1 more for sign, and 1 for 0-termination of string
const unsigned utoa_bufsize=12;
const unsigned utoa_bufsizem1=utoa_bufsize-1;
const unsigned ultoa_bufsize=22;
const unsigned ultoa_bufsizem1=utoa_bufsize-1;
*/

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


// returns n in string [return,num); *num=0 yourself before calling if you want a c_str.  in other words, the sequence [ret,buf) contains the written digits
template <class Int>
char *utoa(char *buf,Int n_) {
  typedef typename signed_for_int<Int>::unsigned_t Uint;
  Uint n=n_;
  if (!n) {
    *--buf='0';
  } else {
    Uint rem;
    // 3digit lookup table, divide by 1000 faster?
    while (n) {
      NDIV10MOD(rem,n);
      *--buf = digit_to_char(rem);
    }
  }
  return buf;
}

// left_pad_0(buf,utoa(buf+bufsz,n)) means that [buf,buf+bufsz) is a left-0 padded seq. of digits.  no 0s are added if utoa is already past buf (you must have ensured that this is valid memory, naturally)
inline void left_pad(char *left,char *buf,char pad='0') {
  while (buf>left)
    *--buf=pad;
  // return buf;
}

template <class Int>
char *utoa_left_pad(char *buf,char *bufend,Int n, char pad='0') {
  char *r=utoa(bufend,n);
  assert(buf<=r);
  left_pad(buf,r,pad);
  return buf;
}

// note: 0 -> 0, but otherwise x000000 -> x (x has no trailing 0s).  same conditions as utoa; [ret,buf) gives the sequence of digits
// useful for floating point fraction output
template <class Uint_>
char *utoa_drop_trailing_0(char *buf,Uint_ n_, unsigned &n_skipped) {
  typedef typename signed_for_int<Uint_>::unsigned_t Uint;
  Uint n=n_;
  n_skipped=0;
  if (!n) {
    *--buf='0';
    return buf;
  } else {
    Uint rem;
    while (n) {
      NDIV10MOD(rem,n);
      if (rem) {
        *--buf = digit_to_char(rem);
        // some non-0 trailing digits; now output all digits.
        while (n) {
          NDIV10MOD(rem,n);
          *--buf = digit_to_char(rem);
        }
        return buf;
      }
      ++n_skipped;
    }
    assert(0);
    return 0;
  }
}

//#include "warning_push.h"
//#pragma GCC diagnostic ignored "-Wtype-limits" // because sign check on itoa<unsigned> is annoying

// desired feature: itoa(unsigned) = utoa(unsigned)
// positive sign: 0 -> +0, 1-> +1.  obviously -n -> -n
template <class Int>
//typename signed_for_int<Int>::original_t instead of Int to give more informative wrong-type message?
char *itoa(char *buf,Int i,bool positive_sign=false) {
  typename signed_for_int<Int>::unsigned_t n=i;
  if (i<0)
    n=-n; //sidesteps 2s complement issue doing this rather than just u=-n.
  char * ret=utoa(buf,n);
  if (i<0) {
    *--ret='-';
  } else if (positive_sign)
    *--ret='+';
  return ret;
}

template <class Int>
char * itoa_left_pad(char *buf,char *bufend,Int i,bool positive_sign=false,char pad='0') {
  typename signed_for_int<Int>::unsigned_t n=i;
  if (i<0) {
    n=-n; //sidesteps 2s complement issue doing this rather than just u=-n.
    *buf='-';
  } else if (positive_sign)
    *buf='+';
  char * r=utoa(bufend,n);
  assert(buf<r);
  left_pad(buf+1,r,pad);
  return buf;
}

template <class Int>
inline std::string utos(Int n) {
  char buf[signed_for_int<Int>::toa_bufsize];
  char *end=buf+signed_for_int<Int>::toa_bufsize;
  char *p=utoa(end,n);
  return std::string(p,end);
}

template <class Int>
inline std::string itos(Int n) {
  char buf[signed_for_int<Int>::toa_bufsize];
  char *end=buf+signed_for_int<Int>::toa_bufsize;
  char *p=itoa(end,n);
  return std::string(p,end);
}

//#include "warning_pop.h"

//returns position of '\0' terminating number written starting at to
template <class Int>
inline char* append_utoa(char *to,typename signed_for_int<Int>::unsigned_t n) {
  char buf[signed_for_int<Int>::toa_bufsize];
  char *end=buf+signed_for_int<Int>::toa_bufsize;
  char *p=itoa(end,n);
  int ns=end-p;
  std::memcpy(to,p,ns);
  to+=ns;
  *to=0;
  return to;
}

//returns position of '\0' terminating number written starting at to
template <class Int>
inline char* append_itoa(char *to,typename signed_for_int<Int>::signed_t n) {
  char buf[signed_for_int<Int>::toa_bufsize];
  char *end=buf+signed_for_int<Int>::toa_bufsize;
  char *p=utoa(end,n);
  int ns=end-p;
  std::memcpy(to,p,ns);
  to+=ns;
  *to=0;
  return to;
}

#endif
