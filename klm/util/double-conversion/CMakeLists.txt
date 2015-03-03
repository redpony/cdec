 

set(klm_util_double_STAT_SRCS
    bignum-dtoa.h
    bignum.h
    cached-powers.h
    diy-fp.h
    double-conversion.h
    fast-dtoa.h
    fixed-dtoa.h
    ieee.h
    strtod.h
    utils.h
    bignum.cc
    bignum-dtoa.cc
    cached-powers.cc
    diy-fp.cc
    double-conversion.cc
    fast-dtoa.cc
    fixed-dtoa.cc
    strtod.cc)

add_library(klm_util_double STATIC ${klm_util_double_STAT_SRCS})

