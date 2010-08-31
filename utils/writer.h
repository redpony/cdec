#ifndef WRITER_H
#define WRITER_H

#include <iostream>

struct Writer
{
  template <class Ch, class Tr,class value_type>
  std::basic_ostream<Ch,Tr>&
  operator()(std::basic_ostream<Ch,Tr>& o,const value_type &l) const {
    return o << l;
  }
};

struct LineWriter
{
  template <class Ch, class Tr,class Label>
  std::basic_ostream<Ch,Tr>&
  operator()(std::basic_ostream<Ch,Tr>& o,const Label &l) const {
    return o << l << std::endl;
  }
};

template <class O, class T,class W> inline
std::ios_base::iostate write_range_iostate(O& o,T begin, T end,W writer,bool multiline=false,bool parens=true,char open_paren='(',char close_paren=')',char space=' ')
{
  static const char *const MULTILINE_SEP="\n";
  if (parens) {
    o << open_paren;
    if (multiline)
      o << MULTILINE_SEP;
  }
  if (multiline) {
    for (;begin!=end;++begin) {
      o << space;
      writer(o,*begin);
      o << MULTILINE_SEP;
    }
  } else {
    for (T i=begin;i!=end;++i) {
      if (i!=begin) o<<space;
      writer(o,*i);
    }
  }
  if (parens) {
    o << close_paren;
    if (multiline)
      o << MULTILINE_SEP;
  }
  return std::ios_base::goodbit;
}


template <class Ib,class Ie,class W=Writer>
struct range_formatter {
  Ib i;
  Ie e;
  W w;
  bool multiline;
  bool parens;
  range_formatter(Ib i,Ie e,W w=W(),bool multiline=false,bool parens=true) :
    i(i),e(e),w(w),multiline(multiline),parens(parens) {}

  template <class Ch, class Tr>
  std::basic_ostream<Ch,Tr> &
  operator()(std::basic_ostream<Ch,Tr> &o) const {
    write_range_iostate(o,i,e,w,multiline,parens);
    return o;
  }

  template <class Ch, class Tr>
  friend inline
  std::basic_ostream<Ch,Tr> &
  operator<<(std::basic_ostream<Ch,Tr> &o,range_formatter<Ib,Ie,W> const& w) {
    return w(o);
  }
};

template <class Ib,class Ie,class W>
range_formatter<Ib,Ie,W>
wrange(Ib i,Ie e,W const& w,bool multiline=false,bool parens=true)
{
  return range_formatter<Ib,Ie,W>(i,e,w,multiline,parens);
}

template <class Ib,class Ie>
range_formatter<Ib,Ie>
prange(Ib i,Ie e,bool multiline=false,bool parens=true)
{
  return range_formatter<Ib,Ie,Writer>(i,e,Writer(),multiline,parens);
}


template <class Ch, class Tr, class T,class W> inline
std::basic_ostream<Ch,Tr> & write_range(std::basic_ostream<Ch,Tr>& o,T begin, T end,W writer,bool multiline=false,bool parens=true,char open_paren='(',char close_paren=')')
{
  write_range_iostate(o,begin,end,writer,multiline,parens,open_paren,close_paren);
  return o;
}

template <class O, class T>
inline std::ios_base::iostate print_range(O& o,T begin,T end,bool multiline=false,bool parens=true,char open_paren='(',char close_paren=')') {
  return  write_range_iostate(o,begin,end,Writer(),multiline,parens,open_paren,close_paren);
}

template <class O, class C>
inline std::ios_base::iostate print_range_i(O& o,C const&c,unsigned from,unsigned to,bool multiline=false,bool parens=true,char open_paren='(',char close_paren=')') {
  return  write_range_iostate(o,c.begin()+from,c.begin()+to,Writer(),multiline,parens,open_paren,close_paren);
}


template <class O>
struct bound_printer
{
    O *po;
    template <class T>
    void operator()(T const& t) const
    {
        *po << t;
    }
};

template <class O>
bound_printer<O>
make_bound_printer(O &o)
{
    bound_printer<O> ret;
    ret.po=&o;
    return ret;
}

template <class W>
struct bound_writer
{
    W const& w;
    bound_writer(W const& w) : w(w) {}
    bound_writer(bound_writer const& o) :w(o.w) {}
    template <class O,class V>
    void operator()(O &o,V const& v) const
    {
        v.print(o,w);
    }
};


template <class W>
bound_writer<W>
make_bound_writer(W const& w)
{
    return bound_writer<W>(w);
}



#endif
