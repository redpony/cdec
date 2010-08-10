#ifndef NT_SPAN_H
#define NT_SPAN_H

#include <iostream>
#include "wordid.h"
#include "tdict.h"

struct Span {
  int l,r;
  Span() : l(-1) {  }
  friend inline std::ostream &operator<<(std::ostream &o,Span const& s) {
    if (s.l<0)
      return o;
    return o<<'<'<<s.l<<','<<s.r<<'>';
  }
};

struct NTSpan {
  Span s;
  WordID nt; // awkward: this is a positive index, used in TD.  but represented as negative in mixed terminal/NT space in rules/hgs.
  NTSpan() : nt(0) {  }
  // prints as possibly empty name (whatever you set of nt,s will show)
  friend inline std::ostream &operator<<(std::ostream &o,NTSpan const& t) {
    if (t.nt>0)
      o<<TD::Convert(t.nt);
    return o << t.s;
  }
};

#endif
