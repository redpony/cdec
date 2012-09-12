#ifndef ALONE_GRAPH__
#define ALONE_GRAPH__

#include "alone/labeled_edge.hh"
#include "search/rule.hh"
#include "search/types.hh"
#include "search/vertex.hh"
#include "util/exception.hh"

#include <boost/noncopyable.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/scoped_array.hpp>

namespace alone {

template <class T> class FixedAllocator : boost::noncopyable {
  public:
    FixedAllocator() : current_(NULL), end_(NULL) {}

    void Init(std::size_t count) {
      assert(!current_);
      array_.reset(new T[count]);
      current_ = array_.get();
      end_ = current_ + count;
    }

    T &operator[](std::size_t idx) {
      return array_.get()[idx];
    }

    T *New() {
      T *ret = current_++;
      UTIL_THROW_IF(ret >= end_, util::Exception, "Allocating past end");
      return ret;
    }

    std::size_t Size() const {
      return end_ - array_.get();
    }

  private:
    boost::scoped_array<T> array_;
    T *current_, *end_;
};

class Graph : boost::noncopyable {
  public:
    typedef LabeledEdge Edge;
    typedef search::Vertex Vertex;

    Graph() {}

    void SetCounts(std::size_t vertices, std::size_t edges) {
      vertices_.Init(vertices);
      edges_.Init(edges);
    }

    Vertex *NewVertex() {
      return vertices_.New();
    }

    std::size_t VertexSize() const { return vertices_.Size(); }

    Vertex &MutableVertex(std::size_t index) {
      return vertices_[index];
    }

    Edge *NewEdge() {      
      return edges_.New();
    }

    std::size_t EdgeSize() const { return edges_.Size(); }

    void SetRoot(Vertex *root) { root_ = root; }

    Vertex &Root() { return *root_; }

  private:
    FixedAllocator<Vertex> vertices_;
    FixedAllocator<Edge> edges_;
    
    Vertex *root_;
};

} // namespace alone

#endif // ALONE_GRAPH__
