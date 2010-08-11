#ifndef THREADLOCAL_H
#define THREADLOCAL_H

#ifndef SETLOCAL_SWAP
# define SETLOCAL_SWAP 0
#endif

#ifdef BOOST_NO_MT

# define THREADLOCAL

#else

#ifdef _MSC_VER

//FIXME: doesn't work with DLLs ... use TLS apis instead (http://www.boost.org/libs/thread/doc/tss.html)
# define THREADLOCAL __declspec(thread)

#else

# define THREADLOCAL __thread

#endif

#endif

#include <algorithm> //swap

// naturally, the below are only thread-safe if value is THREADLOCAL
template <class D>
struct SaveLocal {
    D &value;
    D old_value;
    SaveLocal(D& val) : value(val), old_value(val) {}
    ~SaveLocal() {
#if SETLOCAL_SWAP
      swap(value,old_value);
#else
      value=old_value;
#endif
    }
};

template <class D>
struct SetLocal {
    D &value;
    D old_value;
    SetLocal(D& val,const D &new_value) : value(val), old_value(
#if SETLOCAL_SWAP
      new_value
#else
      val
#endif
      ) {
#if SETLOCAL_SWAP
      swap(value,old_value);
#else
      value=new_value;
#endif
    }
    ~SetLocal() {
#if SETLOCAL_SWAP
      swap(value,old_value);
#else
      value=old_value;
#endif
    }
};


#endif
