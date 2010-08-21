#ifndef LVALUE_PMAP_H
#define LVALUE_PMAP_H

#include <boost/property_map/property_map.hpp>

// i checked: boost provides get and put given []

// lvalue property map pmapname<P> that is: P p; valtype &v=p->name;
#define PMAP_MEMBER_INDIRECT(pmapname,valtype,name) template <class P> struct pmapname {  \
  typedef P key_type; \
  typedef valtype value_type; \
  typedef value_type & reference; \
  typedef boost::lvalue_property_map_tag category;          \
  reference operator[](key_type p) const { return p->name; } \
};

#define PMAP_MEMBER_INDIRECT_2(pmapname,name) template <class P,class R> struct pmapname {    \
  typedef P key_type; \
  typedef R value_type; \
  typedef value_type & reference; \
  typedef boost::lvalue_property_map_tag category; \
  reference operator[](key_type p) const { return p->name; } \
};

#endif
