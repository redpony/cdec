#ifndef SEARCH_DEDUPE__
#define SEARCH_DEDUPE__

#include "lm/state.hh"
#include "search/edge_generator.hh"

#include <boost/pool/object_pool.hpp>
#include <boost/unordered_map.hpp>

namespace search {

class Dedupe {
  public:
    Dedupe() {}

    PartialEdge AllocateEdge(Arity arity) {
      return behind_.AllocateEdge(arity);
    }

    void AddEdge(PartialEdge edge) {
      edge.MutableFlags() = 0;

      uint64_t hash = 0;
      const PartialVertex *v = edge.NT();
      const PartialVertex *v_end = v + edge.GetArity();
      for (; v != v_end; ++v) {
        const void *ptr = v->Identify();
        hash = util::MurmurHashNative(&ptr, sizeof(const void*), hash);
      }
      
      const lm::ngram::ChartState *c = edge.Between();
      const lm::ngram::ChartState *const c_end = c + edge.GetArity() + 1;
      for (; c != c_end; ++c) hash = hash_value(*c, hash);

      std::pair<Table::iterator, bool> ret(table_.insert(std::make_pair(hash, edge)));
      if (!ret.second) FoundDupe(ret.first->second, edge);
    }

    bool Empty() const { return behind_.Empty(); }

    template <class Model, class Output> void Search(Context<Model> &context, Output &output) {
      for (Table::const_iterator i(table_.begin()); i != table_.end(); ++i) {
        behind_.AddEdge(i->second);
      }
      Unpack<Output> unpack(output, *this);
      behind_.Search(context, unpack);
    }

  private:
    void FoundDupe(PartialEdge &table, PartialEdge adding) {
      if (table.GetFlags() & kPackedFlag) {
        Packed &packed = *static_cast<Packed*>(table.GetNote().mut);
        if (table.GetScore() >= adding.GetScore()) {
          packed.others.push_back(adding);
          return;
        }
        Note original(packed.original);
        packed.original = adding.GetNote();
        adding.SetNote(table.GetNote());
        table.SetNote(original);
        packed.others.push_back(table);
        packed.starting = adding.GetScore();
        table = adding;
        table.MutableFlags() |= kPackedFlag;
        return;
      }
      PartialEdge loser;
      if (adding.GetScore() > table.GetScore()) {
        loser = table;
        table = adding;
      } else {
        loser = adding;
      }
      // table is winner, loser is loser...
      packed_.construct(table, loser);
    }

    struct Packed {
      Packed(PartialEdge winner, PartialEdge loser) 
        : original(winner.GetNote()), starting(winner.GetScore()), others(1, loser) {
        winner.MutableNote().vp = this;
        winner.MutableFlags() |= kPackedFlag;
        loser.MutableFlags() &= ~kPackedFlag;
      }
      Note original;
      Score starting;
      std::vector<PartialEdge> others;
    };

    template <class Output> class Unpack {
      public:
        explicit Unpack(Output &output, Dedupe &owner) : output_(output), owner_(owner) {}

        void NewHypothesis(PartialEdge edge) {
          if (edge.GetFlags() & kPackedFlag) {
            Packed &packed = *reinterpret_cast<Packed*>(edge.GetNote().mut);
            edge.SetNote(packed.original);
            edge.MutableFlags() = 0;
            std::size_t copy_size = sizeof(PartialVertex) * edge.GetArity() + sizeof(lm::ngram::ChartState);
            for (std::vector<PartialEdge>::iterator i = packed.others.begin(); i != packed.others.end(); ++i) {
              PartialEdge copy(owner_.AllocateEdge(edge.GetArity()));
              copy.SetScore(edge.GetScore() - packed.starting + i->GetScore());
              copy.MutableFlags() = 0;
              copy.SetNote(i->GetNote());
              memcpy(copy.NT(), edge.NT(), copy_size);
              output_.NewHypothesis(copy);
            }
          }
          output_.NewHypothesis(edge);
        }

        void FinishedSearch() {
          output_.FinishedSearch();
        }

      private:
        Output &output_;
        Dedupe &owner_;
    };

    EdgeGenerator behind_;

    typedef boost::unordered_map<uint64_t, PartialEdge> Table;
    Table table_;

    boost::object_pool<Packed> packed_;

    static const uint16_t kPackedFlag = 1;
};
} // namespace search
#endif // SEARCH_DEDUPE__
