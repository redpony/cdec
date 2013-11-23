#ifndef CDEC_HASH_H
#define CDEC_HASH_H

#include <boost/functional/hash.hpp>

#include "murmur_hash.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SPARSEHASH
# include <sparsehash/dense_hash_map>
# include <sparsehash/dense_hash_set>
# include <sparsehash/sparse_hash_map>
# define SPARSE_HASH_MAP google::sparse_hash_map
# define HASH_MAP google::dense_hash_map
# define HASH_SET google::dense_hash_set
# define HASH_MAP_DELETED(h,deleted) do { (h).set_deleted_key(deleted); } while(0)
# define HASH_MAP_RESERVED(h,empty,deleted) do { (h).set_empty_key(empty); (h).set_deleted_key(deleted); } while(0)
# define HASH_MAP_EMPTY(h,empty) do { (h).set_empty_key(empty); } while(0)
#else
#ifndef HAVE_OLD_CPP
# include <unordered_map>
# include <unordered_set>
#else
# include <tr1/unordered_map>
# include <tr1/unordered_set>
namespace std { using std::tr1::unordered_map; using std::tr1::unordered_set; }
#endif
# define SPARSE_HASH_MAP std::unordered_map
# define HASH_MAP std::unordered_map
# define HASH_SET std::unordered_set
# define HASH_MAP_DELETED(h,deleted)
# define HASH_MAP_RESERVED(h,empty,deleted)
# define HASH_MAP_EMPTY(h,empty)
#endif

#define BOOST_HASHED_MAP(k,v) HASH_MAP<k,v,boost::hash<k> >

namespace {
const unsigned GOLDEN_MEAN_FRACTION=2654435769U;
}

// assumes C is POD
template <class C>
struct murmur_hash
{
  typedef MurmurInt result_type;
  typedef C /*const&*/ argument_type;
  result_type operator()(argument_type const& c) const {
    return MurmurHash((void*)&c,sizeof(c));
  }
};

// murmur_hash_array isn't std guaranteed safe (you need to use string::data())
template <>
struct murmur_hash<std::string>
{
  typedef MurmurInt result_type;
  typedef std::string /*const&*/ argument_type;
  result_type operator()(argument_type const& c) const {
    return MurmurHash(c.data(),c.size());
  }
};

// uses begin(),size() assuming contiguous layout and POD
template <class C>
struct murmur_hash_array
{
  typedef MurmurInt result_type;
  typedef C /*const&*/ argument_type;
  result_type operator()(argument_type const& c) const {
    return MurmurHash(&*c.begin(),c.size()*sizeof(*c.begin()));
  }
};


// adds default val to table if key wasn't found, returns ref to val
template <class H,class K>
typename H::mapped_type & get_default(H &ht,K const& k,typename H::mapped_type const& v) {
  return const_cast<typename H::mapped_type &>(ht.insert(typename H::value_type(k,v)).first->second);
}

// get_or_construct w/ no arg: just use ht[k]
template <class H,class K,class C0>
typename H::mapped_type & get_or_construct(H &ht,K const& k,C0 const& c0) {
  typedef typename H::mapped_type V;
  typedef typename H::value_type KV;
  typename H::iterator_type i=ht.find(k);
  if (i==ht.end()) {
    return const_cast<V &>(ht.insert(KV(k,V(c0))).first->second);
  } else {
    return i->second;
  }
}


// get_or_call (0 arg)
template <class H,class K,class F>
typename H::mapped_type & get_or_call(H &ht,K const& k,F const& f) {
  typedef typename H::mapped_type V;
  typedef typename H::value_type KV;
  typename H::iterator_type i=ht.find(k);
  if (i==ht.end()) {
    return const_cast<V &>(ht.insert(KV(k,f())).first->second);
  } else {
    return i->second;
  }
}

// the below could also return a ref to the mapped max/min.  they have the advantage of not falsely claiming an improvement when an equal value already existed.  otherwise you could just modify the get_default and if equal assume new.
template <class H,class K>
bool improve_mapped_max(H &ht,K const& k,typename H::mapped_type const& v) {
  std::pair<typename H::iterator,bool> inew=ht.insert(typename H::value_type(k,v));
  if (inew.second) return true;
  typedef typename H::mapped_type V;
  V &oldv=const_cast<V&>(inew.first->second);
  if (oldv<v) {
    oldv=v;
    return true;
  }
  return false;
}


// return true if there was no old value.  like ht[k]=v but lets you know whether it was a new addition
template <class H,class K>
bool put(H &ht,K const& k,typename H::mapped_type const& v) {
  std::pair<typename H::iterator,bool> inew=ht.insert(typename H::value_type(k,v));
  if (inew.second)
    return true;
  inew.first->second=v;
  return false;
}

// does not update old value (returns false) if one exists, otherwise add
template <class H,class K>
bool maybe_add(H &ht,K const& k,typename H::mapped_type const& v) {
  std::pair<typename H::iterator,bool> inew=ht.insert(typename H::value_type(k,v));
  return inew.second;
}

// ht[k] must not exist (yet)
template <class H,class K>
void add(H &ht,K const& k,typename H::mapped_type const& v) {
  maybe_add(ht,k,v);
}


template <class H,class K>
bool improve_mapped_min(H &ht,K const& k,typename H::mapped_type const& v) {
  std::pair<typename H::iterator,bool> inew=ht.insert(typename H::value_type(k,v));
  if (inew.second) return true;
  typedef typename H::mapped_type V;
  V &oldv=const_cast<V&>(inew.first->second);
  if (v<oldv) { // the only difference from above
    oldv=v;
    return true;
  }
  return false;
}

#endif
