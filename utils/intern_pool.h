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
  typedef typename I::first_type const& return_type;
  typedef I const& argument_type;
  return_type operator()(I const& i) const {
    return i.first;
  }
};

template <class KeyF,class F,class Arg=typename KeyF::argument_type>
struct compose_indirect {
  typedef Arg *argument_type; // also Arg
  KeyF kf;
  F f;
  typedef typename F::return_type return_type;
  return_type operator()(Arg p) const {
    return f(kf(p));
  }
  template <class V>
  return_type operator()(Arg *p) const {
    return f(kf(*p));
  }

};

/*

template <class F>
struct indirect_function {
  F f;
  explicit indirect_function(F const& f=F()) : f(f) {}
  typedef typename F::return_type return_type;
  template <class V>
  return_type operator()(V *p) const {
    return f(*p);
  }
};
*/

template <class Item,class KeyF=get_key<Item>,class HashKey=boost::hash<typename KeyF::return_type>,class EqKey=std::equal_to<typename KeyF::return_type>, class Pool=boost::object_pool<Item> >
struct intern_pool : Pool {
  KeyF key;
  typedef typename KeyF::return_type Key;
  typedef Item *Handle;
  typedef compose_indirect<KeyF,HashKey,Handle> HashDeep;
  typedef compose_indirect<KeyF,EqKey,Handle> EqDeep;
  typedef HASH_SET<Handle,HashDeep,EqDeep> Canonical;
  typedef typename Canonical::iterator CFind;
  typedef std::pair<CFind,bool> CInsert;
  Canonical canonical;
  void interneq(Handle &i) {
    i=intern(i);
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
