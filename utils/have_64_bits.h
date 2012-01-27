#ifndef HAVE_64_BITS_H
#define HAVE_64_BITS_H

#include <stdint.h>

#undef HAVE_64_BITS

#if INTPTR_MAX == INT32_MAX
# define HAVE_64_BITS 0
#elif INTPTR_MAX >= INT64_MAX
# define HAVE_64_BITS 1
#else
# error "couldn't tell if HAVE_64_BITS from INTPTR_MAX INT32_MAX INT64_MAX"
#endif


#endif
