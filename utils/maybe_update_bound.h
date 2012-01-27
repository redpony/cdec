#ifndef MAYBE_UPDATE_BOUND_H
#define MAYBE_UPDATE_BOUND_H

template <class To,class From>
inline void maybe_increase_max(To &to,const From &from) {
    if (to<from)
        to=from;
}

template <class To,class From>
inline void maybe_decrease_min(To &to,const From &from) {
    if (from<to)
        to=from;
}


#endif
