#ifndef CDEC_HASH_H
#define CDEC_HASH_H

#include "murmur_hash.h"

#include "config.h"
#ifdef HAVE_SPARSEHASH
# include <google/dense_hash_map>
# define HASH_MAP google::dense_hash_map
# define HASH_MAP_RESERVED(h,empty,deleted) do { h.set_empty_key(empty); h.set_deleted_key(deleted); } while(0)
# define HASH_MAP_EMPTY(h,empty) do { h.set_empty_key(empty); } while(0)
#else
# include <tr1/unordered_map>
# define HASH_MAP std::tr1::unordered_map
# define HASH_MAP_RESERVED(h,empty,deleted)
# define HASH_MAP_EMPTY(h,empty)
#endif

#include <boost/functional/hash.hpp>

// assumes C is POD
template <class C>
struct murmur_hash
{
  typedef MurmurInt return_type;
  typedef C /*const&*/ argument_type;
  return_type operator()(argument_type const& c) const {
    return MurmurHash((void*)&c,sizeof(c));
  }
};

// murmur_hash_array isn't std guaranteed safe (you need to use string::data())
template <>
struct murmur_hash<std::string>
{
  typedef MurmurInt return_type;
  typedef std::string /*const&*/ argument_type;
  return_type operator()(argument_type const& c) const {
    return MurmurHash(c.data(),c.size());
  }
};

// uses begin(),size() assuming contiguous layout and POD
template <class C>
struct murmur_hash_array
{
  typedef MurmurInt return_type;
  typedef C /*const&*/ argument_type;
  return_type operator()(argument_type const& c) const {
    return MurmurHash(&*c.begin(),c.size()*sizeof(*c.begin()));
  }
};

// adds default val to table if key wasn't found, returns ref to val
template <class H,class K>
typename H::mapped_type & get_default(H &ht,K const& k,typename H::mapped_type const& v) {
  return const_cast<typename H::data_type &>(ht.insert(typename H::value_type(k,v)).first->second);
}

#endif
