//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#ifndef MURMURHASH3_H_
#define MURMURHASH3_H_

//-----------------------------------------------------------------------------
// Platform-specific functions and macros

// Microsoft Visual Studio

#if defined(_MSC_VER) && (_MSC_VER < 1600)

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;

// Other compilers

#else   // defined(_MSC_VER)

#include <stdint.h>

#endif // !defined(_MSC_VER)

//-----------------------------------------------------------------------------

namespace cdec {

void MurmurHash3_x86_32  ( const void * key, int len, uint32_t seed, void * out );

void MurmurHash3_x86_128 ( const void * key, int len, uint32_t seed, void * out );

void MurmurHash3_x64_128 ( const void * key, int len, uint32_t seed, void * out );

namespace {
  #ifdef __clang__
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wunused-function"
  #endif
  template <unsigned L> inline void cdecMurmurHashNativeBackend(const void * key, int len, uint32_t seed, void * out) {
    MurmurHash3_x86_128(key, len, seed, out);
  }
  template <> inline void cdecMurmurHashNativeBackend<4>(const void * key, int len, uint32_t seed, void * out) {
    MurmurHash3_x64_128(key, len, seed, out);
  }
  #ifdef __clang__
  #pragma clang diagnostic pop
  #endif
} // namespace

inline uint64_t MurmurHash3_64(const void * key, int len, uint32_t seed) {
  uint64_t out[2];
  cdecMurmurHashNativeBackend<sizeof(void*)>(key, len, seed, &out);
  return out[0];
}

inline void MurmurHash3_128(const void * key, int len, uint32_t seed, void * out) {
  cdecMurmurHashNativeBackend<sizeof(void*)>(key, len, seed, out);
}

}

//-----------------------------------------------------------------------------

#endif // _MURMURHASH3_H_
