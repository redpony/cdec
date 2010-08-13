#ifndef NAMED_ENUM_H
#define NAMED_ENUM_H

#ifndef NAMED_ENUM_USE_OPTIONAL
# define NAMED_ENUM_USE_OPTIONAL 0
#endif

//TODO: efficiency - supply map type (e.g. std::map or tr1::unordered_map) for string->int (int->string already fast w/ switch) - then implement iterators that don't assume contiguous ids.
//TODO: hidden (can't convert string->id, but can do reverse) sentinel values.  XX (hidden) and XY (can convert to)
//TODO: bitfield "A|B" strings - note: slightly complicates int->string, as well.
//TODO: option for case-insensitive compare (ctype tolower?)
//TODO: program_options validate method so you can declare po::value<MyEnum> instead of po::value<string>?
//TODO: cout << MyEnum ?
//impossible: (without wrapping in struct) MyEnum(string)

/* named enum (string<->int).  note: inefficient linear search for string->int

in e.h:

#include "named_enum.h"
#define SOME_ENUM(X,t) \
    X(t,FirstValue,) \
    X(t,SecondValue,) \
    X(t,SomeOtherValue,=50) \
    X(t,OneMoreValue,=100) \
#define SOME_ENUM_TYPE MyEnum

DECLARE_NAMED_ENUM(SOME_ENUM)

in e.cc:

DEFINE_NAMED_ENUM(SOME_ENUM)

(or DEFINE_NAMED_ENUM_T(MyEnum,SOME_ENUM) )

#include "e.h"

elsewhere:

#include "e.h"
MyEnum e=GetMyEnum("FirstValue");
string s=GetName(e);
assert(s=="FirstValue");
string usage=MyEnumNames("\n");

 */

#include <stdexcept>
#include <sstream>
#if NAMED_ENUM_USE_OPTIONAL
# include <boost/optional.hpp>
#endif
#include "utoa.h"

inline void throw_enum_error(std::string const& enumtype,std::string const& msg) {
  throw std::runtime_error(enumtype+": "+msg);
}
#if NAMED_ENUM_USE_OPTIONAL
#define NAMED_ENUM_OPTIONAL(x) x
#else
#define NAMED_ENUM_OPTIONAL(x)
#endif

// expansion macro for enum value definition
#define NAMED_ENUM_VALUE(t,name,assign) name assign,

// expansion macro for enum to string conversion
#define NAMED_ENUM_CASE(t,name,assign) case name: return #name;

// expansion macro for enum to string conversion
#define NAMED_ENUM_STRCMP(t,name,assign) if (!std::strcmp(str,#name)) return name;

// expansion macro for enum to optional conversion
#define NAMED_ENUM_STRCMP_OPTIONAL(t,name,assign) if (!std::strcmp(str,#name)) return boost::optional<t>(name);

#define NAMED_ENUM_APPEND_USAGE(t,name,assign) o << #name <<sp; sp=sep;

/// declare the access function and define enum values
#define DECLARE_NAMED_ENUM_T(DEF,EnumType)                    \
  enum EnumType { \
    DEF(NAMED_ENUM_VALUE,EnumType)                            \
  }; \
  const char *GetName(EnumType dummy); \
  EnumType Get ## EnumType (const char *string); \
  inline EnumType Get ## EnumType (std::string const& s) { return Get ## EnumType (s.c_str()); }   \
  std::string EnumType ## Names (char const* sep=","); \
  NAMED_ENUM_OPTIONAL(boost::optional<EnumType> Get ## EnumType ## Optional (const char *string);  inline boost::optional<EnumType> Get ## EnumType ## Optional (std::string const& s) { return Get ## EnumType ## Optional (s.c_str()); })

/// define the access function names
#define DEFINE_NAMED_ENUM_T(DEF,EnumType)       \
   const char *GetName(EnumType value) \
  { \
    switch(value) \
    { \
      DEF(NAMED_ENUM_CASE,EnumType) \
      default: \
        throw_enum_error(#EnumType,"Illegal enum value (no name defined) "+itos((int)value));   \
return ""; /* handle input error */             \
    } \
  } \
   EnumType Get ## EnumType (const char *str) \
  { \
    DEF(NAMED_ENUM_STRCMP,EnumType)                                              \
      throw_enum_error(#EnumType,"Couldn't convert '"+std::string(str)+"' - legal names: "+EnumType ## Names(" ")); \
    return (EnumType)0; /* handle input error */    \
  } \
   std::string EnumType ## Names(char const* sep) {             \
    std::ostringstream o; \
    char const* sp=""; \
    DEF(NAMED_ENUM_APPEND_USAGE,EnumType)        \
    return o.str(); \
    } \
   NAMED_ENUM_OPTIONAL(boost::optional<EnumType> Get ## EnumType ## Optional (const char *str) { DEF(NAMED_ENUM_STRCMP_OPTIONAL,EnumType) return boost::optional<EnumType>(); })

#undef _TYPE
#define DECLARE_NAMED_ENUM_T2(x,y) DECLARE_NAMED_ENUM_T(x,y)
#define DEFINE_NAMED_ENUM_T2(x,y) DEFINE_NAMED_ENUM_T(x,y)
#define DECLARE_NAMED_ENUM(ENUM_DEF) DECLARE_NAMED_ENUM_T2(ENUM_DEF,ENUM_DEF ## _TYPE)
#define DEFINE_NAMED_ENUM(ENUM_DEF) DEFINE_NAMED_ENUM_T2(ENUM_DEF,ENUM_DEF ## _TYPE)
#define MAKE_NAMED_ENUM(ENUM_DEF) \
  DECLARE_NAMED_ENUM(ENUM_DEF)    \
  DEFINE_NAMED_ENUM(ENUM_DEF)

#endif
