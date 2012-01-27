#ifndef STRING_TO_H
#define STRING_TO_H

/*
   may not be any faster than boost::lexical_cast in later incarnations (see http://accu.org/index.php/journals/1375)
   but is slightly simpler.  no wide char or locale.

   X string_to<X>(string);
   string to_string(X);
   X& string_into(string,X &); // note: returns the same ref you passed in, for convenience of use

   default implementation via stringstreams (quite slow, I'm sure)

   fast implementation for string, int<->string, unsigned<->string, float<->string, double<->string

*/

#ifndef USE_FTOA
#define USE_FTOA 1
#endif
#ifndef HAVE_STRTOUL
# define HAVE_STRTOUL 1
#endif

#include <string>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

#include "have_64_bits.h"
#include "utoa.h"
#if USE_FTOA
# include "ftoa.h"
#endif

namespace {
// for faster numeric to/from string.  TODO: separate into optional header
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h> // access to evil (fast) C isspace etc.
#include <limits.h> //strtoul
}

inline void throw_string_to(std::string const& msg,char const* prefix="string_to: ") {
  throw std::runtime_error(prefix+msg);
}

template <class I,class To>
bool try_stream_into(I & i,To &to,bool complete=true)
{
    i >> to;
    if (i.fail()) return false;
    if (complete) {
        char c;
        return !(i >> c);
    }
    return true;
}

template <class Str,class To>
bool try_string_into(Str const& str,To &to,bool complete=true)
{
    std::istringstream i(str);
    return try_stream_into(i,to,complete);
}

template <class Str,class Data> inline
Data & string_into(const Str &str,Data &data)
{
    if (!try_string_into(str,data))
        throw std::runtime_error(std::string("Couldn't convert (string_into): ")+str);
    return data;
}


template <class Data,class Str> inline
Data string_to(const Str &str)
{
    Data ret;
    string_into(str,ret);
    return ret;
}

template <class D> inline
std::string to_string(D const &d)
{
    std::ostringstream o;
    o << d;
    return o.str();
}

inline std::string to_string(unsigned x) {
  return utos(x);
}

inline std::string to_string(int x) {
  return itos(x);
}

inline long strtol_complete(char const* s,int base=10) {
  char *e;
  if (*s) {
    long r=strtol(s,&e,base);
    char c=*e;
    if (!c || isspace(c)) //simplifying assumption: we're happy if there's other stuff in the string, so long as the number ends in a space or eos.  TODO: loop consuming spaces until end?
      return r;
  }
  throw_string_to(s,"Couldn't convert to integer: ");
}

// returns -INT_MAX or INT_MAX if number is too large/small
inline int strtoi_complete_bounded(char const* s,int base=10) {
  long l=strtol_complete(s,base);
  if (l<std::numeric_limits<int>::min())
    return std::numeric_limits<int>::min();
  if (l>std::numeric_limits<int>::max())
    return std::numeric_limits<int>::max();
  return l;
}
#define RANGE_STR(x) #x
#ifdef INT_MIN
# define INTRANGE_STR "[" RANGE_STR(INT_MIN) "," RANGE_STR(INT_MAX) "]"
#else
# define INTRANGE_STR "[-2137483648,2147483647]"
#endif

  // throw if out of int range
inline int strtoi_complete_exact(char const* s,int base=10) {
  long l=strtol_complete(s,base);
  if (l<std::numeric_limits<int>::min() || l>std::numeric_limits<int>::max())
    throw_string_to(s,"Out of range for int " INTRANGE_STR ": ");
  return l;
}

#if HAVE_LONGER_LONG
inline int& string_into(std::string const& s,int &x) {
  x=strtoi_complete_exact(s.c_str());
  return x;
}
inline int& string_into(char const* s,int &x) {
  x=strtoi_complete_exact(s);
  return x;
}
#endif

inline long& string_into(std::string const& s,long &x) {
  x=strtol_complete(s.c_str());
  return x;
}
inline long& string_into(char const* s,long &x) {
  x=strtol_complete(s);
  return x;
}


//FIXME: preprocessor separation for tokens int<->unsigned int, long<->unsigned long, strtol<->strtoul ?  massive code duplication
inline unsigned long strtoul_complete(char const* s,int base=10) {
  char *e;
  if (*s) {
#if HAVE_STRTOUL
    unsigned long r=strtoul(s,&e,base);
#else
//    unsigned long r=strtol(s,&e,base); //FIXME: not usually safe
    unsigned long r;
    sscanf(s,"%ul",&r);
#endif
    char c=*e;
    if (!c || isspace(c)) //simplifying assumption: we're happy if there's other stuff in the string, so long as the number ends in a space or eos.  TODO: loop consuming spaces until end?
      return r;
  }
  throw_string_to(s,"Couldn't convert to integer: ");
}

inline unsigned strtou_complete_bounded(char const* s,int base=10) {
  unsigned long l=strtoul_complete(s,base);
  if (l<std::numeric_limits<unsigned>::min())
    return std::numeric_limits<unsigned>::min();
  if (l>std::numeric_limits<unsigned>::max())
    return std::numeric_limits<unsigned>::max();
  return l;
}

#ifdef UINT_MIN
# define UINTRANGE_STR "[" RANGE_STR(UINT_MIN) "," RANGE_STR(UINT_MAX) "]"
#else
# define UINTRANGE_STR "[0,4,294,967,295]"
#endif

  // throw if out of int range
inline unsigned strtou_complete_exact(char const* s,int base=10) {
  unsigned long l=strtoul_complete(s,base);
  if (l<std::numeric_limits<unsigned>::min() || l>std::numeric_limits<unsigned>::max())
    throw_string_to(s,"Out of range for uint " UINTRANGE_STR ": ");
  return l;
}

#if HAVE_LONGER_LONG
inline unsigned& string_into(std::string const& s,unsigned &x) {
  x=strtou_complete_exact(s.c_str());
  return x;
}
inline unsigned& string_into(char const* s,unsigned &x) {
  x=strtou_complete_exact(s);
  return x;
}
#endif

inline unsigned long& string_into(std::string const& s,unsigned long &x) {
  x=strtoul_complete(s.c_str());
  return x;
}
inline unsigned long& string_into(char const* s,unsigned long &x) {
  x=strtoul_complete(s);
  return x;
}

//FIXME: end code duplication


/* 9 decimal places needed to avoid rounding error in float->string->float.  17 for double->string->double
   in terms of usable decimal places, there are 6 for float and 15 for double
 */
inline std::string to_string_roundtrip(float x) {
  char buf[17];
  return std::string(buf,buf+sprintf(buf,"%.9g",x));
}
inline std::string to_string(float x) {
#if USE_FTOA
  return ftos(x);
#else
  char buf[15];
  return std::string(buf,buf+sprintf(buf,"%.7g",x));
#endif
}
inline std::string to_string_roundtrip(double x) {
  char buf[32];
  return std::string(buf,buf+sprintf(buf,"%.17g",x));
}
inline std::string to_string(double x) {
#if USE_FTOA
  return ftos(x);
#else
  char buf[30];
  return std::string(buf,buf+sprintf(buf,"%.15g",x));
#endif
}

inline double& string_into(char const* s,double &x) {
  x=std::atof(s);
  return x;
}
inline float& string_into(char const* s,float &x) {
  x=std::atof(s);
  return x;
}

inline double& string_into(std::string const& s,double &x) {
  x=std::atof(s.c_str());
  return x;
}
inline float& string_into(std::string const& s,float &x) {
  x=std::atof(s.c_str());
  return x;
}


template <class Str>
bool try_string_into(Str const& str,Str &to,bool complete=true)
{
    str=to;
    return true;
}

inline std::string const& to_string(std::string const& d)
{
    return d;
}

template <class Str>
Str const& string_to(Str const &s)
{
    return s;
}

template <class Str>
Str & string_into(Str const &s,Str &d)
{
    return d=s;
}

/*

template <class Str,class Data,class size_type> inline
void substring_into(const Str &str,size_type pos,size_type n,Data &data)
{
//    std::istringstream i(str,pos,n); // doesn't exist!
    std::istringstream i(str.substr(pos,n));
    if (!(i>>*data))
        throw std::runtime_error("Couldn't convert (string_into): "+str);
}

template <class Data,class Str,class size_type> inline
Data string_to(const Str &str,size_type pos,size_type n)
{
    Data ret;
    substring_into(str,pos,n,ret);
    return ret;
}

*/



#endif
