#ifndef log_add_hh
#define log_add_hh

#include <limits>
#include <iostream>
#include <cassert>
#include <cmath>

template <typename T>
struct Log
{
    static T zero() { return -std::numeric_limits<T>::infinity(); } 

    static T add(T l1, T l2)
    {
        if (l1 == zero()) return l2;
        if (l1 > l2) 
            return l1 + std::log(1 + exp(l2 - l1));
        else
            return l2 + std::log(1 + exp(l1 - l2));
    }

    static T subtract(T l1, T l2)
    {
        //std::assert(l1 >= l2);
        return l1 + log(1 - exp(l2 - l1));
    }
};

#endif
