#ifndef INT_OR_POINTER_H
#define INT_OR_POINTER_H

// if you ever wanted to store a discriminated union of pointer/integer without an extra boolean flag, this will do it, assuming your pointers are never odd.

// check lsb for expected tag?
#ifndef IOP_CHECK_LSB
# define IOP_CHECK_LSB 1
#endif
#if IOP_CHECK_LSB
# define iop_assert(x) assert(x)
#else
# define iop_assert(x)
#endif

#include <assert.h>
#include <iostream>

template <class Pointed=void,class Int=size_t>
struct IntOrPointer {
  typedef Pointed pointed_type;
  typedef Int integer_type;
  typedef Pointed *value_type;
  typedef IntOrPointer<Pointed,Int> self_type;
  IntOrPointer(int j) { *this=j; }
  IntOrPointer(size_t j) { *this=j; }
  IntOrPointer(value_type v) { *this=v; }
  bool is_integer() const { return i&1; }
  bool is_pointer() const { return !(i&1); }
  value_type & pointer() { return p; }
  const value_type & pointer() const { iop_assert(is_pointer()); return p; }
  integer_type integer() const { iop_assert(is_integer()); return i >> 1; }
  void set_integer(Int j) { i=2*j+1; }
  void set_pointer(value_type p_) { p=p_;iop_assert(is_pointer()); }
  void operator=(unsigned j) { i = 2*(integer_type)j+1; }
  void operator=(int j) { i = 2*(integer_type)j+1; }
  template <class C>
  void operator=(C j) { i = 2*(integer_type)j+1; }
  void operator=(value_type v) { p=v; }
  IntOrPointer() {}
  IntOrPointer(const self_type &s) : p(s.p) {}
  void operator=(const self_type &s) { p=s.p; }
  template <class C>
  bool operator ==(C* v) const { return p==v; }
  template <class C>
  bool operator ==(const C* v) const { return p==v; }
  template <class C>
  bool operator ==(C j) const { return integer() == j; }
  bool operator ==(self_type s) const { return p==s.p; }
  bool operator !=(self_type s) const { return p!=s.p; }
  template <class O> void print(O&o) const
  {
    if (is_integer())
      o << integer();
    else {
      o << "0x" << std::hex << (size_t)pointer() << std::dec;
    }
  }
  friend inline std::ostream& operator<<(std::ostream &o,self_type const& s) {
    s.print(o); return o;
  }
protected:
  union {
    value_type p; // must be even (guaranteed unless you're pointing at packed chars)
    integer_type i; // stored as 2*data+1, so only has half the range (one less bit) of a normal integer_type
  };
};


#endif
