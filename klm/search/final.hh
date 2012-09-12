#ifndef SEARCH_FINAL__
#define SEARCH_FINAL__

#include "search/arity.hh"
#include "search/types.hh"

#include <boost/array.hpp>

namespace search {

class Edge;

class Final {
  public:
    typedef boost::array<const Final*, search::kMaxArity> ChildArray;

    void Reset(Score bound, const Edge &from, const Final &left, const Final &right) {
      bound_ = bound;
      from_ = &from;
      children_[0] = &left;
      children_[1] = &right;
    }

    const ChildArray &Children() const { return children_; }

    const Edge &From() const { return *from_; }

    Score Bound() const { return bound_; }

  private:
    Score bound_;

    const Edge *from_;

    ChildArray children_;
};

} // namespace search

#endif // SEARCH_FINAL__
