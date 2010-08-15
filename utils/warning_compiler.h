#ifndef WARNING_COMPILER_HPP
#define WARNING_COMPILER_HPP

#ifndef HAVE_GCC_4_4
#undef HAVE_DIAGNOSTIC_PUSH
#if __GNUC__ > 4 || __GNUC__==4 && __GNUC_MINOR__ > 3
# define HAVE_GCC_4_4 1
# define HAVE_DIAGNOSTIC_PUSH 1
#else
# define HAVE_GCC_4_4 0
# define HAVE_DIAGNOSTIC_PUSH 0
#endif
#endif

#endif
