#ifndef _HG_KBEST_H_
#define _HG_KBEST_H_

#include <vector>
#include <utility>
#ifndef HAVE_OLD_CPP
# include <unordered_set>
#else
# include <tr1/unordered_set>
namespace std { using std::tr1::unordered_set; }
#endif

#include <boost/shared_ptr.hpp>
#include <boost/type_traits.hpp>

#include "wordid.h"
#include "hg.h"

namespace KBest {
  // default, don't filter any derivations from the k-best list
  template<typename Dummy>
  struct NoFilter {
    bool operator()(const Dummy&) {
      return false;
    }
  };

  // optional, filter unique yield strings
  struct FilterUnique {
    std::unordered_set<std::vector<WordID>, boost::hash<std::vector<WordID> > > unique;

    bool operator()(const std::vector<WordID>& yield) {
      return !unique.insert(yield).second;
    }
  };

  // utility class to lazily create the k-best derivations from a forest, uses
  // the lazy k-best algorithm (Algorithm 3) from Huang and Chiang (IWPT 2005)
  template<typename T,  // yield type (returned by Traversal)
           typename Traversal,
           typename DerivationFilter = NoFilter<T>,
           typename WeightType = prob_t,
           typename WeightFunction = EdgeProb>
  struct KBestDerivations {
    KBestDerivations(const Hypergraph& hg,
                     const size_t k,
                     const Traversal& tf = Traversal(),
                     const WeightFunction& wf = WeightFunction()) :
      traverse(tf), w(wf), g(hg), nds(g.nodes_.size()), k_prime(k) {}

    ~KBestDerivations() {
      for (unsigned i = 0; i < freelist.size(); ++i)
        delete freelist[i];
    }

    struct Derivation {
      Derivation(const HG::Edge& e,
                 const SmallVectorInt& jv,
                 const WeightType& w,
                 const SparseVector<double>& f) :
        edge(&e),
        j(jv),
        score(w),
        feature_values(f) {}

      // dummy constructor, just for query
      Derivation(const HG::Edge& e,
                 const SmallVectorInt& jv) : edge(&e), j(jv) {}

      T yield;
      const HG::Edge* const edge;
      const SmallVectorInt j;
      const WeightType score;
      const SparseVector<double> feature_values;
    };
    struct HeapCompare {
      bool operator()(const Derivation* a, const Derivation* b) const {
        return a->score < b->score;
      }
    };
    struct DerivationCompare {
      bool operator()(const Derivation* a, const Derivation* b) const {
        return a->score > b->score;
      }
    };

    struct EdgeHandle {
      Derivation const* d;
      explicit EdgeHandle(Derivation const* d) : d(d) {  }
//      operator bool() const { return d->edge; }
      operator HG::Edge const* () const { return d->edge; }
//      HG::Edge const * operator ->() const { return d->edge; }
    };

    EdgeHandle operator()(unsigned t,unsigned taili,EdgeHandle const& parent) const {
      return EdgeHandle(nds[t].D[parent.d->j[taili]]);
    }

    std::string derivation_tree(Derivation const& d,bool indent=true,int show_mask=Hypergraph::SPAN|Hypergraph::RULE,int maxdepth=0x7FFFFFFF,int depth=0) const {
      return d.edge->derivation_tree(*this,EdgeHandle(&d),indent,show_mask,maxdepth,depth);
    }

    struct DerivationUniquenessHash {
      size_t operator()(const Derivation* d) const {
        size_t x = 5381;
        x = ((x << 5) + x) ^ d->edge->id_;
        for (unsigned i = 0; i < d->j.size(); ++i)
          x = ((x << 5) + x) ^ d->j[i];
        return x;
      }
    };
    struct DerivationUniquenessEquals {
      bool operator()(const Derivation* a, const Derivation* b) const {
        return (a->edge == b->edge) && (a->j == b->j);
      }
    };
    typedef std::vector<Derivation*> CandidateHeap;
    typedef std::vector<Derivation*> DerivationList;
    typedef std::unordered_set<
       const Derivation*, DerivationUniquenessHash, DerivationUniquenessEquals> UniqueDerivationSet;

    struct NodeDerivationState {
      CandidateHeap cand;
      DerivationList D;
      DerivationFilter filter;
      UniqueDerivationSet ds;
      explicit NodeDerivationState(const DerivationFilter& f = DerivationFilter()) : filter(f) {}
    };

    Derivation* LazyKthBest(unsigned v, unsigned k) {
      NodeDerivationState& s = GetCandidates(v);
      CandidateHeap& cand = s.cand;
      DerivationList& D = s.D;
      DerivationFilter& filter = s.filter;
      bool add_next = true;
      while (D.size() <= k) {
        if (add_next && D.size() > 0) {
          const Derivation* d = D.back();
          LazyNext(d, &cand, &s.ds);
        }
        add_next = false;

        while (!add_next && cand.size() > 0) {
          std::pop_heap(cand.begin(), cand.end(), HeapCompare());
          Derivation* d = cand.back();
          cand.pop_back();
          std::vector<const T*> ants(d->edge->Arity());
          for (unsigned j = 0; j < ants.size(); ++j)
            ants[j] = &LazyKthBest(d->edge->tail_nodes_[j], d->j[j])->yield;
          traverse(*d->edge, ants, &d->yield);
          if (!filter(d->yield)) {
            D.push_back(d);
            add_next = true;
          } else {
            // just because a node already derived a string (or whatever
            // equivalent derivation class), you need to add its successors
            // to the node's candidate pool
            LazyNext(d, &cand, &s.ds);
          }
        }
        if (!add_next)
          break;
      }
      if (k < D.size()) return D[k]; else return NULL;
    }

  private:
    // creates a derivation object with all fields set but the yield
    // the yield is computed in LazyKthBest before the derivation is added to D
    // returns NULL if j refers to derivation numbers larger than the
    // antecedent structure define
    Derivation* CreateDerivation(const HG::Edge& e, const SmallVectorInt& j) {
      WeightType score = w(e);
      SparseVector<double> feats = e.feature_values_;
      for (int i = 0; i < e.Arity(); ++i) {
        const Derivation* ant = LazyKthBest(e.tail_nodes_[i], j[i]);
        if (!ant) { return NULL; }
        score *= ant->score;
        feats += ant->feature_values;
      }
      freelist.push_back(new Derivation(e, j, score, feats));
      return freelist.back();
    }

    NodeDerivationState& GetCandidates(unsigned v) {
      NodeDerivationState& s = nds[v];
      if (!s.D.empty() || !s.cand.empty()) return s;

      const Hypergraph::Node& node = g.nodes_[v];
      for (unsigned i = 0; i < node.in_edges_.size(); ++i) {
        const HG::Edge& edge = g.edges_[node.in_edges_[i]];
        SmallVectorInt jv(edge.Arity(), 0);
        Derivation* d = CreateDerivation(edge, jv);
        assert(d);
        s.cand.push_back(d);
      }

      unsigned effective_k = s.cand.size();
      if (boost::is_same<DerivationFilter,NoFilter<T> >::value) {
        // if there's no filter you can use this optimization
        effective_k = std::min(k_prime, s.cand.size());
      }
      const typename CandidateHeap::iterator kth = s.cand.begin() + effective_k;
      std::nth_element(s.cand.begin(), kth, s.cand.end(), DerivationCompare());
      s.cand.resize(effective_k);
      std::make_heap(s.cand.begin(), s.cand.end(), HeapCompare());

      return s;
    }

    void LazyNext(const Derivation* d, CandidateHeap* cand, UniqueDerivationSet* ds) {
      for (unsigned i = 0; i < d->j.size(); ++i) {
        SmallVectorInt j = d->j;
        ++j[i];
        const Derivation* ant = LazyKthBest(d->edge->tail_nodes_[i], j[i]);
        if (ant) {
          Derivation query_unique(*d->edge, j);
          if (ds->count(&query_unique) == 0) {
            Derivation* new_d = CreateDerivation(*d->edge, j);
            if (new_d) {
              cand->push_back(new_d);
              std::push_heap(cand->begin(), cand->end(), HeapCompare());
#ifdef NDEBUG
              ds->insert(new_d).second;  // insert into uniqueness set
#else
              bool inserted = ds->insert(new_d).second;  // insert into uniqueness set
              assert(inserted);
#endif
            }
          }
        }
      }
    }

    const Traversal traverse;
    const WeightFunction w;
    const Hypergraph& g;
    std::vector<NodeDerivationState> nds;
    std::vector<Derivation*> freelist;
    const size_t k_prime;
  };
}

#endif
