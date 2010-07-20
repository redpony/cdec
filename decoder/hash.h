#ifndef CDEC_HASH_H
#define CDEC_HASH_H

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


#endif
