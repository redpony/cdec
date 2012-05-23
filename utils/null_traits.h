#ifndef NULL_TRAITS_H
#define NULL_TRAITS_H

template <class V>
struct null_traits {
  static V xnull; //TODO: maybe take out default null and make ppl explicitly define?  they may be surprised that they need to when they include a header lib that uses null_traits
};
// global bool is_null(V const& v)

// definitely override this, and possibly set_null and is_null.  that's the point.
template <class V>
V null_traits<V>::xnull;
//TODO: are we getting single init of the static null object?

template <class V>
void set_null(V &v) {
  v=null_traits<V>::xnull;
}

template <class V>
void is_null(V const& v) {
  return v==null_traits<V>::xnull;
}


#endif
