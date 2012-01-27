#ifndef NULL_DELETER_H
#define NULL_DELETER_H

struct null_deleter {
    void operator()(void*) const {}
    void operator()(void const*) const {}
};

#endif
