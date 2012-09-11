#ifndef SEARCH_FINAL__
#define SEARCH_FINAL__

#include "search/rule.hh"
#include "search/types.hh"

#include <boost/array.hpp>

namespace search {

class Final {
  public:
    typedef boost::array<const Final*, search::kMaxArity> ChildArray;

    void Reset(Score bound, const Rule &from, const Final &left, const Final &right) {
      bound_ = bound;
      from_ = &from;
      children_[0] = &left;
      children_[1] = &right;
    }

    const ChildArray &Children() const { return children_; }

    unsigned int ChildCount() const { return from_->Arity(); }

    const Rule &From() const { return *from_; }

    Score Bound() const { return bound_; }

  private:
    Score bound_;

    const Rule *from_;

    ChildArray children_;
};

} // namespace search

#endif // SEARCH_FINAL__
