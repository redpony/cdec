#ifndef NT_SPAN_H
#define NT_SPAN_H

#include <iostream>
#include "wordid.h"
#include "tdict.h"

struct Span {
  int l,r;
  Span() : l(-1) {  }
  bool is_null() const { return l<0; }
  void print(std::ostream &o,char const* for_null="") const {
    if (is_null())
      o<<for_null;
    else
      o<<'<'<<l<<','<<r<<'>';
  }
  friend inline std::ostream &operator<<(std::ostream &o,Span const& s) {
    s.print(o);return o;
  }
};

struct NTSpan {
  Span s;
  WordID nt; // awkward: this is a positive index, used in TD.  but represented as negative in mixed terminal/NT space in rules/hgs.
  NTSpan() : nt(0) {  }
  // prints as possibly empty name (whatever you set of nt,s will show)
  void print(std::ostream &o,char const* for_span_null="_",char const* for_null="") const {
    if (nt>0) {
      o<<TD::Convert(nt);
      s.print(o,for_span_null);
    } else
      s.print(o,for_null);
  }
  friend inline std::ostream &operator<<(std::ostream &o,NTSpan const& t) {
    t.print(o);return o;
  }
};

#endif
