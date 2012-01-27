#ifndef FTOA_H
#define FTOA_H


//TODO: for fractional digits/non-sci, determine the right amount of left padding (more if the whole number is indeed <1, to keep the significant digits), less if sci notation and/or mantissa has sig. digits (don't want N before . and N after!)

#ifndef FTOA_ROUNDTRIP
# define FTOA_ROUNDTRIP 1
#endif

#ifndef FTOA_DEBUG
# define FTOA_DEBUG 0
#endif

#ifndef FTOA_USE_SPRINTF
#define FTOA_USE_SPRINTF 0
#endif

#if FTOA_DEBUG
# define FTOAassert(x) assert(x)
# define DBFTOA(x) std::cerr<<"\nFTOA " <<__func__<<"("<<__LINE__<<"): " #x "="<<x<<"\n"
# define DBFTOA2(x0,x1) std::cerr<<"\nFTOA " <<__func__<<"("<<__LINE__<<"): " #x0 "="<<x0<<" " #x1 "="<<x1 <<"\n"
#else
# define FTOAassert(x)
# define DBFTOA(x)
# define DBFTOA2(x0,x1)
#endif

/* DECIMAL_FOR_WHOLE ; ftos(123)
   0 ; 123
   1 ; 123
   2 ; 123.
     ; ftos(0) is always just "0" (not "0.0")
     ; ftos(.01)
   0 ; .01
   1 ; 0.01
   2 ; 0.01

*/

#ifndef DECIMAL_FOR_WHOLE
# define DECIMAL_FOR_WHOLE 1
#endif

#include <limits>
#include <stdint.h>
#include <iostream>
#include <cmath>
#include <assert.h>
#include <cstdio>
#include "utoa.h"
#include "nan.h"

template <class Float>
struct ftoa_traits {
};

//eP10,
// sigd decimal places normally printed, roundtripd needed so that round-trip float->string->float is identity

#define DEFINE_FTOA_TRAITS(FLOATT,INTT,sigd,roundtripd,small,large,used,P10)   \
template <> \
struct ftoa_traits<FLOATT> { \
  typedef INTT int_t; \
  typedef u ## INTT uint_t; \
  typedef FLOATT float_t; \
  enum { digits10=std::numeric_limits<INTT>::digits10, chars_block=P10, usedig=used, sigdig=sigd, roundtripdig=roundtripd, bufsize=roundtripdig+7 }; \
  static const double pow10_block = 1e ## P10; \
  static const float_t small_f = small; \
  static const float_t large_f = large; \
  static inline int sprintf(char *buf,double f) { return std::sprintf(buf,"%." #used "g",f); } \
  static inline int sprintf_sci(char *buf,double f) { return std::sprintf(buf,"%." #used "e",f); } \
  static inline int sprintf_nonsci(char *buf,double f) { return std::sprintf(buf,"%." #used "f",f); } \
  static inline uint_t fracblock(double frac) { FTOAassert(frac>=0 && frac<1); double f=frac*pow10_block;uint_t i=(uint_t)f;FTOAassert(i<pow10_block);return i; } \
  static inline uint_t rounded_fracblock(double frac) { FTOAassert(frac>=0 && frac<1); double f=frac*pow10_block;uint_t i=(uint_t)(f+.5);FTOAassert(i<pow10_block);return i; }  \
  static inline float_t mantexp10(float_t f,int &exp) { float_t e=std::log10(f); float_t ef=std::floor(e); exp=ef;  return f/std::pow((float_t)10,ef); } \
  static inline bool use_sci_abs(float_t fa) { return fa<small || fa>large; } \
  static inline bool use_sci(float_t f) { return use_sci_abs(std::fabs(f)); }   \
};
//TODO: decide on computations in double (would hurt long double) or in native float type - any advantage?  more precision is usually better.

//10^22 = 0x1.0f0cf064dd592p73 is the largest exactly representable power of 10 in the binary64 format.  but round down to 18 so int64_t can hold it.

#if FTOA_ROUNDTRIP
#define DEFINE_FTOA_TRAITS_ROUNDTRIP(FLOATT,INTT,sigd,roundtripd,small,large) DEFINE_FTOA_TRAITS(FLOATT,INTT,sigd,roundtripd,small,large,roundtripd,roundtripd)
#else
#define DEFINE_FTOA_TRAITS_ROUNDTRIP(FLOATT,INTT,sigd,roundtripd,small,large) DEFINE_FTOA_TRAITS(FLOATT,INTT,sigd,roundtripd,small,large,sigd,sigd)
#endif

DEFINE_FTOA_TRAITS_ROUNDTRIP(double,int64_t,15,17,1e-5,1e8)
//i've heard that 1e10 is fine for float.  but we only have 1e9 (9 decimal places) in int32.
DEFINE_FTOA_TRAITS_ROUNDTRIP(float,int32_t,6,9,1e-3,1e8)


template <class F>
inline void ftoa_error(F f,char const* msg="") {
  using namespace std;
  cerr<<"ftoa error: "<<msg<<" f="<<f<<endl;
  assert(!"ftoa error");
}

// all of the below prepend and return new cursor.  null terminate yourself (like itoa/utoa)

//possibly empty string for ~0 (no sci notation fallback).  left padded with the right number of 0s (tricky).  [ret,p) are the digits.
template <class F>
char *prepend_pos_frac_digits(char *p,F f) {
  FTOAassert(f<1 && f >0);
  typedef ftoa_traits<F> FT;
  //repeat if very small???  nah, require sci notation to take care of it.
  typename FT::uint_t i=FT::rounded_fracblock(f);
  DBFTOA2(f,i);
  if (i>0) {
    unsigned n_skipped;
    char *d=utoa_drop_trailing_0(p,i,n_skipped);
    char *b=p-FT::chars_block+n_skipped;
    FTOAassert(b<=d);
    left_pad(b,d,'0');
    return b;
  } else {
    return p;
  }
}

template <class F>
char *append_pos_frac_digits(char *p,F f) { // '0' right-padded, nul terminated, return position of nul.  [p,ret) are the digits
  if (f==0) {
    *p++='0';
    return p;
  }
  FTOAassert(f<1 && f >0);
  typedef ftoa_traits<F> FT;
  //repeat if very small???  nah, require sci notation to take care of it.
  typename FT::uint_t i=FT::rounded_fracblock(f);
  DBFTOA2(f,i);
  if (i>0) {
    char *e=p+FT::chars_block;
    utoa_left_pad(p,e,i,'0');
    *e=0;
    return e;
  } else {
    *p=0;
    return p;
  }
}

template <class F>
inline char *prepend_pos_frac(char *p,F f) {
  FTOAassert(f<1 && f>=0);
  if (f==0) {
    *--p='0';
    return p;
  }
  p=prepend_pos_frac_digits(p,f);
  *--p='.';
  if (DECIMAL_FOR_WHOLE>0)
    *--p='0';
  return p;
}

template <class F>
inline char *append_pos_frac(char *p,F f) {
  DBFTOA(f);
  if (DECIMAL_FOR_WHOLE>0)
    *p++='0';
  *p++='.';
  return append_pos_frac_digits(p,f);
}

template <class F>
inline char *prepend_frac(char *p,F f,bool positive_sign=false) {
  FTOAassert(f<1 && f>-1);
  if (f==0)
    *--p='0';
  else if (f<0) {
    p=prepend_pos_frac(p,-f);
    *--p='-';
  } else {
    p=prepend_pos_frac(p,f);
    if (positive_sign)
      *--p='+';
  }
  return p;
}


template <class F>
inline char *append_sign(char *p,F f,bool positive_sign=false) {
  if (f<0) {
    *p++='-';
  } else if (positive_sign)
    *p++='+';
  return p;
}

template <class F>
inline char *append_frac(char *p,F f,bool positive_sign=false) {
  FTOAassert(f<1 && f>-1);
  if (f==0) {
    *p++='0';
    return p;
  } else if (f<0) {
    *p++='-';
    return append_pos_frac(p,-f);
  }
  if (positive_sign) {
    *p++='+';
    return append_pos_frac(p,f);
  }

}


//append_frac, append_pos_sci, append_sci.  notice these are all composed according to a pattern (but reversing order of composition in pre vs app).  or can implement with copy through buffer

/* will switch to sci notation if integer part is too big for the int type. but for very small values, will simply display 0 (i.e. //TODO: find out log10 and leftpad 0s then convert rest) */
template <class F>
char *prepend_pos_nonsci(char *p,F f) {
  typedef ftoa_traits<F> FT;
  typedef typename FT::uint_t uint_t;
  DBFTOA(f);
  FTOAassert(f>0);
  if (f>std::numeric_limits<uint_t>::max())
    return prepend_pos_sci(p,f);
  //which is faster - modf is weird and returns negative frac part if f is negative.  while we could deal with this using fabs, we instead only handle positive here (put - sign in front and negate, then call us) - ?
#if 0
  F intpart;
  F frac=std::modf(f,&intpart);
  uint_t u=intpart;
#else
  uint_t u=f;
  F frac=f-u;
#endif
  DBFTOA2(u,frac);
  if (frac == 0) {
    if (DECIMAL_FOR_WHOLE>1)
      *--p='.';
  } else {
    p=prepend_pos_frac_digits(p,frac);
    *--p='.';
  }
  if (u==0) {
    if (DECIMAL_FOR_WHOLE>0)
      *--p='0';
  } else
    p=utoa(p,u);
  return p;
}

// modify p; return true if handled
template <class F>
inline bool prepend_0_etc(char *&p,F f,bool positive_sign=false) {
  if (f==0) {
    *--p='0';
    return true;
  }
  if (is_nan(f)) {
    p-=3;
    p[0]='N';p[1]='A';p[2]='N';
    return true;
  }
  if (is_pos_inf(f)) {
    p-=3;
    p[0]='I';p[1]='N';p[2]='F';
    if (positive_sign)
      *--p='+';
    return true;
  }
  if (is_neg_inf(f)) {
    p-=4;
    p[0]='-';p[1]='I';p[2]='N';p[3]='F';
    return true;
  }
  return false;
}

template <class F>
inline char *prepend_nonsci(char *p,F f,bool positive_sign=false) {
  if (prepend_0_etc(p,f,positive_sign)) return p;
  if (f<0) {
    p=prepend_pos_nonsci(p,-f);
    *--p='-';
  } else {
    p=prepend_pos_nonsci(p,f);
    if (positive_sign)
      *--p='+';
  }
  return p;
}

template <class F>
inline char *prepend_pos_sci(char *p,F f,bool positive_sign_exp=false) {
  FTOAassert(f>0);
  typedef ftoa_traits<F> FT;
  int e10;
  F mant=FT::mantexp10(f,e10);
  DBFTOA(f);
  DBFTOA2(mant,e10);
  FTOAassert(mant<10.00001);
  if (mant>=10.) {
    ++e10;
    mant*=.1;
  } else if (mant < 1.) {
    --e10;
    mant*=10;
  }
  p=itoa(p,e10,positive_sign_exp);
  *--p='e';
  return prepend_pos_nonsci(p,mant);
}

template <class F>
inline char *prepend_sci(char *p,F f,bool positive_sign_mant=false,bool positive_sign_exp=false) {
  if (prepend_0_etc(p,f,positive_sign_mant)) return p;
  if (f==0)
    *--p='0';
  else if (f<0) {
    p=prepend_pos_sci(p,-f,positive_sign_exp);
    *--p='-';
  } else {
    p=prepend_pos_sci(p,f,positive_sign_exp);
    if (positive_sign_mant)
      *--p='+';
  }
  return p;
}

template <class F>
inline char *append_nonsci(char *p,F f,bool positive_sign=false) {
  if (positive_sign&&f>=0) *p++='+';
  return p+ftoa_traits<F>::sprintf_nonsci(p,f);
}

template <class F>
inline char *append_sci(char *p,F f,bool positive_sign=false) {
  if (positive_sign&&f>=0) *p++='+';
  return p+ftoa_traits<F>::sprintf_sci(p,f);
}

template <class F>
inline char *append_ftoa(char *p,F f,bool positive_sign=false) {
  if (positive_sign&&f>=0) *p++='+';
  return p+ftoa_traits<F>::sprintf(p,f);
}

template <class F>
inline char *prepend_ftoa(char *p,F f)
{
  typedef ftoa_traits<F> FT;
  return FT::use_sci(f) ? prepend_sci(p,f) : prepend_nonsci(p,f);
}

template <class F>
inline std::string ftos_append(F f) {
  typedef ftoa_traits<F> FT;
  char buf[FT::bufsize];
  return std::string(buf,append_ftoa(buf,f));
}

template <class F>
inline std::string ftos_prepend(F f) {
  typedef ftoa_traits<F> FT;
  char buf[FT::bufsize];
  char *end=buf+FT::bufsize;
  return std::string(prepend_ftoa(end,f),end);
}


template <class F>
inline std::string ftos(F f) {
#if 0
  // trust RVO?  no extra copies?
  return FTOA_USE_SPRINTF ? ftos_append(f) : ftos_prepend(f);
#else
  typedef ftoa_traits<F> FT;
  char buf[FT::bufsize];
  if (FTOA_USE_SPRINTF) {
    return std::string(buf,append_ftoa(buf,f));
  } else {
    char *end=buf+FT::bufsize;
    return std::string(prepend_ftoa(end,f),end);
  }
#endif
}

namespace {
  const int ftoa_bufsize=30;
  char ftoa_outbuf[ftoa_bufsize];
}

// not even THREADLOCAL - don't use.
inline char *static_ftoa(float f)
{
  if (FTOA_USE_SPRINTF) {
    append_ftoa(ftoa_outbuf,f);
    return ftoa_outbuf;
  } else {
    char *end=ftoa_outbuf+ftoa_bufsize;
    return prepend_ftoa(end,f);
  }
}


#endif
