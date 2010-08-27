#ifndef INTERN_POOL_H
#define INTERN_POOL_H

#define DEBUG_INTERN_POOL(x) x

/* to "intern" a string in lisp is to make a symbol from it (a pointer to a canonical copy whose pointer can be equality-compared/hashed directly with other interned things).  we take an Item that has a key part and some mutable parts (that aren't in its identity), and we hash-by-value the key part to map to a canonical on-heap Item - and we use a boost object pool to allocate them */

//FIXME: actually store function object state (assumed stateless so far)

#include <boost/pool/object_pool.hpp>
#include "hash.h"
//#include "null_traits.h"
#include <functional>

template <class I>
struct get_key { // default accessor for I = like pair<key,val>
  typedef typename I::first_type const& result_type;
  typedef I const& argument_type;
  result_type operator()(I const& i) const {
    return i.first;
  }
};

// Arg type should be the non-pointer version.  this saves me from using boost type traits to remove_pointer.  f may be binary or unary
template <class KeyF,class F,class Arg=typename KeyF::argument_type>
struct compose_indirect {
  typedef Arg *argument_type; // we also accept Arg &
  KeyF kf;
  F f;
  typedef typename F::result_type result_type;
  result_type operator()(Arg const& p) const {
    return f(kf(p));
  }
  result_type operator()(Arg & p) const {
    return f(kf(p));
  }
  result_type operator()(Arg * p) const {
    return f(kf(*p));
  }
  template <class V>
  result_type operator()(V const& v) const {
    return f(kf(*v));
  }

  result_type operator()(Arg const& a1,Arg const& a2) const {
    return f(kf(a1),kf(a2));
  }
  result_type operator()(Arg & a1,Arg & a2) const {
    return f(kf(a1),kf(a2));
  }
  result_type operator()(Arg * a1,Arg * a2) const {
    return f(kf(*a1),kf(*a2));
  }
  template <class V,class W>
  result_type operator()(V const& v,W const&w) const {
    return f(kf(*v),kf(*w));
  }


};

/*

template <class F>
struct indirect_function {
  F f;
  explicit indirect_function(F const& f=F()) : f(f) {}
  typedef typename F::result_type result_type;
  template <class V>
  result_type operator()(V *p) const {
    return f(*p);
  }
};
*/

template <class Item,class KeyF=get_key<Item>,class HashKey=boost::hash<typename KeyF::result_type>,class EqKey=std::equal_to<typename KeyF::result_type>, class Pool=boost::object_pool<Item> >
struct intern_pool : Pool {
  KeyF key;
  typedef typename KeyF::result_type Key;
  typedef Item *Handle;
  typedef compose_indirect<KeyF,HashKey,Item> HashDeep;
  typedef compose_indirect<KeyF,EqKey,Item> EqDeep;
  typedef HASH_SET<Handle,HashDeep,EqDeep> Canonical;
  typedef typename Canonical::iterator CFind;
  typedef std::pair<CFind,bool> CInsert;
  Canonical canonical;
  bool interneq(Handle &i) { // returns true if i is newly interned, false if it already existed
    CInsert i_new=canonical.insert(i);
    i=*i_new.first;
    return i_new.second;
  }
// inherited: Handle construct(...)
  Handle construct_fresh() { return Pool::construct(); }
  Handle intern(Handle i) { // (maybe invalidating i, returning a valid canonical handle (pointer)
    CInsert i_new=canonical.insert(i);
    if (i_new.second)
      return i;
    else {
      free(i);
      return *i_new->first;
    }
  }
  void destroy_interned(Handle i) {
    DEBUG_INTERN_POOL(assert(canonical.find(i)!=canonical.end()));
    canonical.erase(i);
    destroy(i);
  }
  bool destroy_fresh(Handle i) {
    DEBUG_INTERN_POOL(assert(canonical.find(i)!=canonical.end()||*canonical.find(i)!=i)); // i is a constructed item not yet interned.
    destroy(i);
  }
  void destroy_both(Handle i) { // i must have come from this pool.  may be interned, or not.  destroy both the noninterned and interned.
    if (!destroy_if_interned(i)) destroy(i);
  }
  // destroy intern(i) if it exists.  return true if it existed AND its address was i.  otherwise return false (whether or not a value-equal item existed and was destroyed)
  bool destroy_if_interned(Handle i) {
    CFind f=canonical.find(i);
    if (f!=canonical.end()) {
      Handle interned=*f;
      canonical.erase(f);
      destroy(f);
      if (f==i) return true;
    }
    return false;
  }

  intern_pool() {
    HASH_MAP_EMPTY(canonical,(Handle)0);
  }
};



#endif
