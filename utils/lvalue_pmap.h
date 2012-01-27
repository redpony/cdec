#ifndef LVALUE_PMAP_H
#define LVALUE_PMAP_H

#include <boost/property_map/property_map.hpp>

// i checked: boost provides get and put given [] - but it's not being found by ADL so instead i define them myself

// lvalue property map pmapname<P> that is: P p; valtype &v=p->name;
#define PMAP_MEMBER_INDIRECT(pmapname,valtype,name) template <class P> struct pmapname {  \
  typedef P key_type; \
  typedef valtype value_type; \
  typedef value_type & reference; \
  typedef boost::lvalue_property_map_tag category;          \
  reference operator[](key_type p) const { return p->name; } \
  typedef pmapname<P> self_type; \
  friend inline value_type const& get(self_type const&,key_type p) { return p->name; } \
  friend inline void put(self_type &,key_type p,value_type const& v) { p->name = v; }             \
};

#define PMAP_MEMBER_INDIRECT_2(pmapname,name) template <class P,class R> struct pmapname {    \
  typedef P key_type; \
  typedef R value_type; \
  typedef value_type & reference; \
  typedef boost::lvalue_property_map_tag category; \
  reference operator[](key_type p) const { return p->name; } \
  typedef pmapname<P,R> self_type;                                                      \
  friend inline value_type const& get(self_type const&,key_type p) { return p->name; } \
  friend inline void put(self_type &,key_type p,value_type const& v) { p->name = v; }             \
};

#endif
