#ifndef SEARCH_VERTEX_GENERATOR__
#define SEARCH_VERTEX_GENERATOR__

#include "search/edge.hh"
#include "search/edge_generator.hh"

#include <boost/pool/pool.hpp>
#include <boost/unordered_map.hpp>

#include <queue>

namespace lm {
namespace ngram {
class ChartState;
} // namespace ngram
} // namespace lm

namespace search {

template <class Model> class Context;
class ContextBase;
class Final;

class VertexGenerator {
  public:
    template <class Model> VertexGenerator(Context<Model> &context, Vertex &gen);

    PartialEdge *MallocPartialEdge() { return static_cast<PartialEdge*>(partial_edge_pool_.malloc()); }
    void FreePartialEdge(PartialEdge *value) { partial_edge_pool_.free(value); }

    void NewHypothesis(const lm::ngram::ChartState &state, const Edge &from, const PartialEdge &partial);

  private:
    // Parallel structure to VertexNode.  
    struct Trie {
      Trie() : under(NULL) {}

      VertexNode *under;
      boost::unordered_map<uint64_t, Trie> extend;
    };

    Trie &FindOrInsert(Trie &node, uint64_t added, const lm::ngram::ChartState &state, unsigned char left, bool left_full, unsigned char right, bool right_full);

    Final *CompleteTransition(Trie &node, const lm::ngram::ChartState &state, const Edge &from, const PartialEdge &partial);

    ContextBase &context_;

    std::vector<EdgeGenerator> edges_;

    struct LessByTop : public std::binary_function<const EdgeGenerator *, const EdgeGenerator *, bool> {
      bool operator()(const EdgeGenerator *first, const EdgeGenerator *second) const {
        return first->Top() < second->Top();
      }
    };

    typedef std::priority_queue<EdgeGenerator*, std::vector<EdgeGenerator*>, LessByTop> Generate;
    Generate generate_;

    Trie root_;

    typedef boost::unordered_map<uint64_t, Final*> Existing;
    Existing existing_;

    int to_pop_;

    boost::pool<> partial_edge_pool_;
};

} // namespace search
#endif // SEARCH_VERTEX_GENERATOR__
