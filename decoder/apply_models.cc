////TODO: keep model state in forest?

//TODO: (for many nonterminals, or multi-rescoring pass) either global
//best-first, or group by (NT,span) - use prev forest outside as a (admissable,
//if models are a subset and weights are same) heuristic

#include "apply_models.h"

#include <vector>
#include <algorithm>
#ifndef HAVE_OLD_CPP
# include <unordered_map>
# include <unordered_set>
#else
# include <tr1/unordered_map>
# include <tr1/unordered_set>
namespace std { using std::tr1::unordered_map; using std::tr1::unordered_set; }
#endif

#include <boost/functional/hash.hpp>

#include "verbose.h"
#include "hg.h"
#include "ff.h"
#include "ffset.h"

#define NORMAL_CP 1
#define FAST_CP 2
#define FAST_CP_2 3

using namespace std;

struct Candidate;
typedef SmallVectorInt JVector;
typedef vector<Candidate*> CandidateHeap;
typedef vector<Candidate*> CandidateList;

// default vector size (* sizeof string is memory used)
static const size_t kRESERVE_NUM_NODES = 500000ul;

// life cycle: candidates are created, placed on the heap
// and retrieved by their estimated cost, when they're
// retrieved, they're incorporated into the +LM hypergraph
// where they also know the head node index they are
// attached to.  After they are added to the +LM hypergraph
// vit_prob_ and est_prob_ fields may be updated as better
// derivations are found (this happens since the successor's
// of derivation d may have a better score- they are
// explored lazily).  However, the updates don't happen
// when a candidate is in the heap so maintaining the heap
// property is not an issue.
struct Candidate {
  int node_index_;                     // -1 until incorporated
                                       // into the +LM forest
  const Hypergraph::Edge* in_edge_;    // in -LM forest
  Hypergraph::Edge out_edge_;
  FFState state_;
  const JVector j_;
  prob_t vit_prob_;            // these are fixed until the cand
                               // is popped, then they may be updated
  prob_t est_prob_;

  Candidate(const Hypergraph::Edge& e,
            const JVector& j,
            const Hypergraph& out_hg,
            const vector<CandidateList>& D,
            const FFStates& node_states,
            const SentenceMetadata& smeta,
            const ModelSet& models,
            bool is_goal) :
      node_index_(-1),
      in_edge_(&e),
      j_(j) {
    InitializeCandidate(out_hg, smeta, D, node_states, models, is_goal);
  }

  // used to query uniqueness
  Candidate(const Hypergraph::Edge& e,
            const JVector& j) : in_edge_(&e), j_(j) {}

  bool IsIncorporatedIntoHypergraph() const {
    return node_index_ >= 0;
  }

  void InitializeCandidate(const Hypergraph& out_hg,
                           const SentenceMetadata& smeta,
                           const vector<vector<Candidate*> >& D,
                           const FFStates& node_states,
                           const ModelSet& models,
                           const bool is_goal) {
    const Hypergraph::Edge& in_edge = *in_edge_;
    out_edge_.rule_ = in_edge.rule_;
    out_edge_.feature_values_ = in_edge.feature_values_;
    out_edge_.i_ = in_edge.i_;
    out_edge_.j_ = in_edge.j_;
    out_edge_.prev_i_ = in_edge.prev_i_;
    out_edge_.prev_j_ = in_edge.prev_j_;
    Hypergraph::TailNodeVector& tail = out_edge_.tail_nodes_;
    tail.resize(j_.size());
    prob_t p = prob_t::One();
    // cerr << "\nEstimating application of " << in_edge.rule_->AsString() << endl;
    for (int i = 0; i < tail.size(); ++i) {
      const Candidate& ant = *D[in_edge.tail_nodes_[i]][j_[i]];
      assert(ant.IsIncorporatedIntoHypergraph());
      tail[i] = ant.node_index_;
      p *= ant.vit_prob_;
    }
    prob_t edge_estimate = prob_t::One();
    if (is_goal) {
      assert(tail.size() == 1);
      const FFState& ant_state = node_states[tail.front()];
      models.AddFinalFeatures(ant_state, &out_edge_, smeta);
    } else {
      models.AddFeaturesToEdge(smeta, out_hg, node_states, &out_edge_, &state_, &edge_estimate);
    }
    vit_prob_ = out_edge_.edge_prob_ * p;
    est_prob_ = vit_prob_ * edge_estimate;
  }
};

ostream& operator<<(ostream& os, const Candidate& cand) {
  os << "CAND[";
  if (!cand.IsIncorporatedIntoHypergraph()) { os << "PENDING "; }
  else { os << "+LM_node=" << cand.node_index_; }
  os << " edge=" << cand.in_edge_->id_;
  os << " j=<";
  for (int i = 0; i < cand.j_.size(); ++i)
    os << (i==0 ? "" : " ") << cand.j_[i];
  os << "> vit=" << log(cand.vit_prob_);
  os << " est=" << log(cand.est_prob_);
  return os << ']';
}

struct HeapCandCompare {
  bool operator()(const Candidate* l, const Candidate* r) const {
    return l->est_prob_ < r->est_prob_;
  }
};

struct EstProbSorter {
  bool operator()(const Candidate* l, const Candidate* r) const {
    return l->est_prob_ > r->est_prob_;
  }
};

// the same candidate <edge, j> can be added multiple times if
// j is multidimensional (if you're going NW in Manhattan, you
// can first go north, then west, or you can go west then north)
// this is a hash function on the relevant variables from
// Candidate to enforce this.
struct CandidateUniquenessHash {
  size_t operator()(const Candidate* c) const {
    size_t x = 5381;
    x = ((x << 5) + x) ^ c->in_edge_->id_;
    for (int i = 0; i < c->j_.size(); ++i)
      x = ((x << 5) + x) ^ c->j_[i];
    return x;
  }
};

struct CandidateUniquenessEquals {
  bool operator()(const Candidate* a, const Candidate* b) const {
    return (a->in_edge_ == b->in_edge_) && (a->j_ == b->j_);
  }
};

typedef unordered_set<const Candidate*, CandidateUniquenessHash, CandidateUniquenessEquals> UniqueCandidateSet;
typedef unordered_map<FFState, Candidate*, boost::hash<FFState> > State2Node;

class CubePruningRescorer {

public:
  CubePruningRescorer(const ModelSet& m,
                      const SentenceMetadata& sm,
                      const Hypergraph& i,
                      int pop_limit,
                      Hypergraph* o,
                      int s = NORMAL_CP ) :
      models(m),
      smeta(sm),
      in(i),
      out(*o),
      D(in.nodes_.size()),
      pop_limit_(pop_limit),
      strategy_(s){
    if (!SILENT) cerr << "  Applying feature functions (cube pruning, pop_limit = " << pop_limit_ << ')' << endl;
    node_states_.reserve(kRESERVE_NUM_NODES);
  }

  void Apply() {
    int num_nodes = in.nodes_.size();
    assert(num_nodes >= 2);
    int goal_id = num_nodes - 1;
    int pregoal = goal_id - 1;
    assert(in.nodes_[pregoal].out_edges_.size() == 1);
    if (!SILENT) cerr << "    ";
    int has = 0;
    for (int i = 0; i < in.nodes_.size(); ++i) {
      if (!SILENT) {
        int needs = (50 * i / in.nodes_.size());
        while (has < needs) { cerr << '.'; ++has; }
      }
      if (strategy_==NORMAL_CP){
        KBest(i, i == goal_id);
      }
      if (strategy_==FAST_CP){
        KBestFast(i, i == goal_id);
      }
      if (strategy_==FAST_CP_2){
        KBestFast2(i, i == goal_id);
      }
    }
    if (!SILENT) {
      cerr << endl;
      cerr << "  Best path: " << log(D[goal_id].front()->vit_prob_)
           << "\t" << log(D[goal_id].front()->est_prob_) << endl;
    }
    out.PruneUnreachable(D[goal_id].front()->node_index_);
    FreeAll();
  }

 private:
  void FreeAll() {
    for (int i = 0; i < D.size(); ++i) {
      CandidateList& D_i = D[i];
      for (int j = 0; j < D_i.size(); ++j)
        delete D_i[j];
    }
    D.clear();
  }

  void IncorporateIntoPlusLMForest(Candidate* item, State2Node* s2n, CandidateList* freelist) {
    Hypergraph::Edge* new_edge = out.AddEdge(item->out_edge_);
    new_edge->edge_prob_ = item->out_edge_.edge_prob_;
    Candidate*& o_item = (*s2n)[item->state_];
    if (!o_item) o_item = item;

    int& node_id = o_item->node_index_;
    if (node_id < 0) {
      Hypergraph::Node* new_node = out.AddNode(in.nodes_[item->in_edge_->head_node_].cat_);
      node_states_.push_back(item->state_);
      node_id = new_node->id_;
    }
#if 0
    Hypergraph::Node* node = &out.nodes_[node_id];
    out.ConnectEdgeToHeadNode(new_edge, node);
#else
    out.ConnectEdgeToHeadNode(new_edge, node_id);
#endif
    // update candidate if we have a better derivation
    // note: the difference between the vit score and the estimated
    // score is the same for all items with a common residual DP
    // state
    if (item->vit_prob_ > o_item->vit_prob_) {
      assert(o_item->state_ == item->state_);    // sanity check!
      o_item->est_prob_ = item->est_prob_;
      o_item->vit_prob_ = item->vit_prob_;
    }
    if (item != o_item) freelist->push_back(item);
  }

  void KBest(const int vert_index, const bool is_goal) {
    // cerr << "KBest(" << vert_index << ")\n";
    CandidateList& D_v = D[vert_index];
    assert(D_v.empty());
    const Hypergraph::Node& v = in.nodes_[vert_index];
    // cerr << "  has " << v.in_edges_.size() << " in-coming edges\n";
    const vector<int>& in_edges = v.in_edges_;
    CandidateHeap cand;
    CandidateList freelist;
    cand.reserve(in_edges.size());
    UniqueCandidateSet unique_cands;
    for (int i = 0; i < in_edges.size(); ++i) {
      const Hypergraph::Edge& edge = in.edges_[in_edges[i]];
      const JVector j(edge.tail_nodes_.size(), 0);
      cand.push_back(new Candidate(edge, j, out, D, node_states_, smeta, models, is_goal));
      bool is_new = unique_cands.insert(cand.back()).second;
      assert(is_new);  // these should all be unique!
    }
//    cerr << "  making heap of " << cand.size() << " candidates\n";
    make_heap(cand.begin(), cand.end(), HeapCandCompare());
    State2Node state2node;   // "buf" in Figure 2
    int pops = 0;
    while(!cand.empty() && pops < pop_limit_) {
      pop_heap(cand.begin(), cand.end(), HeapCandCompare());
      Candidate* item = cand.back();
      cand.pop_back();
      // cerr << "POPPED: " << *item << endl;
      PushSucc(*item, is_goal, &cand, &unique_cands);
      IncorporateIntoPlusLMForest(item, &state2node, &freelist);
      ++pops;
    }
    D_v.resize(state2node.size());
    int c = 0;
    for (State2Node::iterator i = state2node.begin(); i != state2node.end(); ++i)
      D_v[c++] = i->second;
    sort(D_v.begin(), D_v.end(), EstProbSorter());
    // cerr << "  expanded to " << D_v.size() << " nodes\n";

    for (int i = 0; i < cand.size(); ++i)
      delete cand[i];
    // freelist is necessary since even after an item merged, it still stays in
    // the unique set so it can't be deleted til now
    for (int i = 0; i < freelist.size(); ++i)
      delete freelist[i];
  }

  void KBestFast(const int vert_index, const bool is_goal) {
	  // cerr << "KBest(" << vert_index << ")\n";
	  CandidateList& D_v = D[vert_index];
	  assert(D_v.empty());
	  const Hypergraph::Node& v = in.nodes_[vert_index];
	  // cerr << " has " << v.in_edges_.size() << " in-coming edges\n";
	  const vector<int>& in_edges = v.in_edges_;
	  CandidateHeap cand;
	  CandidateList freelist;
	  cand.reserve(in_edges.size());
	  //init with j<0,0> for all rules-edges that lead to node-(NT-span)
	  for (int i = 0; i < in_edges.size(); ++i) {
		  const Hypergraph::Edge& edge = in.edges_[in_edges[i]];
		  const JVector j(edge.tail_nodes_.size(), 0);
		  cand.push_back(new Candidate(edge, j, out, D, node_states_, smeta, models, is_goal));
	  }
	  // cerr << " making heap of " << cand.size() << " candidates\n";
	  make_heap(cand.begin(), cand.end(), HeapCandCompare());
	  State2Node state2node; // "buf" in Figure 2
	  int pops = 0;
	  while(!cand.empty() && pops < pop_limit_) {
		  pop_heap(cand.begin(), cand.end(), HeapCandCompare());
		  Candidate* item = cand.back();
		  cand.pop_back();
		  // cerr << "POPPED: " << *item << endl;

		  PushSuccFast(*item, is_goal, &cand);
		  IncorporateIntoPlusLMForest(item, &state2node, &freelist);
		  ++pops;
	  }
	  D_v.resize(state2node.size());
	  int c = 0;
	  for (State2Node::iterator i = state2node.begin(); i != state2node.end(); ++i){
		  D_v[c++] = i->second;
		  // cerr << "MERGED: " << *i->second << endl;
	  }
	  //cerr <<"Node id: "<< vert_index<< endl;
	  //#ifdef MEASURE_CA
	  // cerr << "countInProcess (pop/tot): node id: " << vert_index << " (" << count_in_process_pop << "/" << count_in_process_tot << ")"<<endl;
	  // cerr << "countAtEnd (pop/tot): node id: " << vert_index << " (" << count_at_end_pop << "/" << count_at_end_tot << ")"<<endl;
	  //#endif
	  sort(D_v.begin(), D_v.end(), EstProbSorter());

	  // cerr << " expanded to " << D_v.size() << " nodes\n";

	  for (int i = 0; i < cand.size(); ++i)
		  delete cand[i];
	  // freelist is necessary since even after an item merged, it still stays in
	  // the unique set so it can't be deleted til now
	  for (int i = 0; i < freelist.size(); ++i)
		  delete freelist[i];
  }

  void KBestFast2(const int vert_index, const bool is_goal) {
	  // cerr << "KBest(" << vert_index << ")\n";
	  CandidateList& D_v = D[vert_index];
	  assert(D_v.empty());
	  const Hypergraph::Node& v = in.nodes_[vert_index];
	  // cerr << " has " << v.in_edges_.size() << " in-coming edges\n";
	  const vector<int>& in_edges = v.in_edges_;
	  CandidateHeap cand;
	  CandidateList freelist;
	  cand.reserve(in_edges.size());
	  UniqueCandidateSet unique_accepted;
	  //init with j<0,0> for all rules-edges that lead to node-(NT-span)
	  for (int i = 0; i < in_edges.size(); ++i) {
		  const Hypergraph::Edge& edge = in.edges_[in_edges[i]];
		  const JVector j(edge.tail_nodes_.size(), 0);
		  cand.push_back(new Candidate(edge, j, out, D, node_states_, smeta, models, is_goal));
	  }
	  // cerr << " making heap of " << cand.size() << " candidates\n";
	  make_heap(cand.begin(), cand.end(), HeapCandCompare());
	  State2Node state2node; // "buf" in Figure 2
	  int pops = 0;
	  while(!cand.empty() && pops < pop_limit_) {
		  pop_heap(cand.begin(), cand.end(), HeapCandCompare());
		  Candidate* item = cand.back();
		  cand.pop_back();
                  bool is_new = unique_accepted.insert(item).second;
		  assert(is_new); // these should all be unique!
		  // cerr << "POPPED: " << *item << endl;

		  PushSuccFast2(*item, is_goal, &cand, &unique_accepted);
		  IncorporateIntoPlusLMForest(item, &state2node, &freelist);
		  ++pops;
	  }
	  D_v.resize(state2node.size());
	  int c = 0;
	  for (State2Node::iterator i = state2node.begin(); i != state2node.end(); ++i){
		  D_v[c++] = i->second;
		  // cerr << "MERGED: " << *i->second << endl;
	  }
	  //cerr <<"Node id: "<< vert_index<< endl;
	  //#ifdef MEASURE_CA
	  // cerr << "countInProcess (pop/tot): node id: " << vert_index << " (" << count_in_process_pop << "/" << count_in_process_tot << ")"<<endl;
	  // cerr << "countAtEnd (pop/tot): node id: " << vert_index << " (" << count_at_end_pop << "/" << count_at_end_tot << ")"<<endl;
	  //#endif
	  sort(D_v.begin(), D_v.end(), EstProbSorter());

	  // cerr << " expanded to " << D_v.size() << " nodes\n";

	  for (int i = 0; i < cand.size(); ++i)
		  delete cand[i];
	  // freelist is necessary since even after an item merged, it still stays in
	  // the unique set so it can't be deleted til now
	  for (int i = 0; i < freelist.size(); ++i)
		  delete freelist[i];
  }

  void PushSucc(const Candidate& item, const bool is_goal, CandidateHeap* pcand, UniqueCandidateSet* cs) {
    CandidateHeap& cand = *pcand;
    for (int i = 0; i < item.j_.size(); ++i) {
      JVector j = item.j_;
      ++j[i];
      if (j[i] < D[item.in_edge_->tail_nodes_[i]].size()) {
        Candidate query_unique(*item.in_edge_, j);
        if (cs->count(&query_unique) == 0) {
          Candidate* new_cand = new Candidate(*item.in_edge_, j, out, D, node_states_, smeta, models, is_goal);
          cand.push_back(new_cand);
          push_heap(cand.begin(), cand.end(), HeapCandCompare());
          bool is_new = cs->insert(new_cand).second;
          assert(is_new);  // insert into uniqueness set, sanity check
        }
      }
    }
  }

  //PushSucc following unique ancestor generation function
  void PushSuccFast(const Candidate& item, const bool is_goal, CandidateHeap* pcand){
	  CandidateHeap& cand = *pcand;
	  for (int i = 0; i < item.j_.size(); ++i) {
		  JVector j = item.j_;
		  ++j[i];
		  if (j[i] < D[item.in_edge_->tail_nodes_[i]].size()) {
			  Candidate* new_cand = new Candidate(*item.in_edge_, j, out, D, node_states_, smeta, models, is_goal);
			  cand.push_back(new_cand);
			  push_heap(cand.begin(), cand.end(), HeapCandCompare());
		  }
		  if(item.j_[i]!=0){
			  return;
		  }
	  }
  }

  //PushSucc only if all ancest Cand are added
  void PushSuccFast2(const Candidate& item, const bool is_goal, CandidateHeap* pcand, UniqueCandidateSet* ps){
	  CandidateHeap& cand = *pcand;
	  for (int i = 0; i < item.j_.size(); ++i) {
		  JVector j = item.j_;
		  ++j[i];
		  if (j[i] < D[item.in_edge_->tail_nodes_[i]].size()) {
			  Candidate query_unique(*item.in_edge_, j);
			  if (HasAllAncestors(&query_unique,ps)) {
				  Candidate* new_cand = new Candidate(*item.in_edge_, j, out, D, node_states_, smeta, models, is_goal);
				  cand.push_back(new_cand);
				  push_heap(cand.begin(), cand.end(), HeapCandCompare());
			  }
		  }
	  }
  }

  bool HasAllAncestors(const Candidate* item, UniqueCandidateSet* cs){
	  for (int i = 0; i < item->j_.size(); ++i) {
		  JVector j = item->j_;
		  --j[i];
		  if (j[i] >=0) {
			  Candidate query_unique(*item->in_edge_, j);
			  if (cs->count(&query_unique) == 0) {
				  return false;
			  }
		  }
	  }
	  return true;
  }

  const ModelSet& models;
  const SentenceMetadata& smeta;
  const Hypergraph& in;
  Hypergraph& out;

  vector<CandidateList> D;   // maps nodes in in-HG to the
                             // equivalent nodes (many due to state
                             // splits) in the out-HG.
  FFStates node_states_;  // for each node in the out-HG what is
                             // its q function value?
  const int pop_limit_;
 const int strategy_;       //switch Cube Pruning strategy: 1 normal, 2 fast (alg 2), 3 fast_2 (alg 3). (see: Gesmundo A., Henderson J,. Faster Cube Pruning, IWSLT 2010)
};

struct NoPruningRescorer {
  NoPruningRescorer(const ModelSet& m, const SentenceMetadata &sm, const Hypergraph& i, Hypergraph* o) :
      models(m),
      smeta(sm),
      in(i),
      out(*o),
      nodemap(i.nodes_.size()) {
    if (!SILENT) cerr << "  Rescoring forest (full intersection)\n";
    node_states_.reserve(kRESERVE_NUM_NODES);
  }

  typedef unordered_map<FFState, int, boost::hash<FFState> > State2NodeIndex;

  void ExpandEdge(const Hypergraph::Edge& in_edge, bool is_goal, State2NodeIndex* state2node) {
    const int arity = in_edge.Arity();
    Hypergraph::TailNodeVector ends(arity);
    for (int i = 0; i < arity; ++i)
      ends[i] = nodemap[in_edge.tail_nodes_[i]].size();

    Hypergraph::TailNodeVector tail_iter(arity, 0);
    bool done = false;
    while (!done) {
      Hypergraph::TailNodeVector tail(arity);
      for (int i = 0; i < arity; ++i)
        tail[i] = nodemap[in_edge.tail_nodes_[i]][tail_iter[i]];
      Hypergraph::Edge* new_edge = out.AddEdge(in_edge, tail);
      FFState head_state;
      if (is_goal) {
        assert(tail.size() == 1);
        const FFState& ant_state = node_states_[tail.front()];
        models.AddFinalFeatures(ant_state, new_edge,smeta);
      } else {
        prob_t edge_estimate; // this is a full intersection, so we disregard this
        models.AddFeaturesToEdge(smeta, out, node_states_, new_edge, &head_state, &edge_estimate);
      }
      int& head_plus1 = (*state2node)[head_state];
      if (!head_plus1) {
        head_plus1 = out.AddNode(in_edge.rule_->GetLHS())->id_ + 1;
        node_states_.push_back(head_state);
        nodemap[in_edge.head_node_].push_back(head_plus1 - 1);
      }
      const int head_index = head_plus1 - 1;
      out.ConnectEdgeToHeadNode(new_edge->id_, head_index);

      int ii = 0;
      for (; ii < arity; ++ii) {
        ++tail_iter[ii];
        if (tail_iter[ii] < ends[ii]) break;
        tail_iter[ii] = 0;
      }
      done = (ii == arity);
    }
  }

  void ProcessOneNode(const int node_num, const bool is_goal) {
    State2NodeIndex state2node;
    const Hypergraph::Node& node = in.nodes_[node_num];
    for (int i = 0; i < node.in_edges_.size(); ++i) {
      const Hypergraph::Edge& edge = in.edges_[node.in_edges_[i]];
      ExpandEdge(edge, is_goal, &state2node);
    }
  }

  void Apply() {
    int num_nodes = in.nodes_.size();
    int goal_id = num_nodes - 1;
    int pregoal = goal_id - 1;
    assert(in.nodes_[pregoal].out_edges_.size() == 1);
    if (!SILENT) cerr << "    ";
    int has = 0;
    for (int i = 0; i < in.nodes_.size(); ++i) {
      if (!SILENT) {
        int needs = (50 * i / in.nodes_.size());
        while (has < needs) { cerr << '.'; ++has; }
      }
      ProcessOneNode(i, i == goal_id);
    }
    if (!SILENT) cerr << endl;
  }

 private:
  const ModelSet& models;
  const SentenceMetadata& smeta;
  const Hypergraph& in;
  Hypergraph& out;

  vector<vector<int> > nodemap;
  FFStates node_states_;  // for each node in the out-HG what is
                             // its q function value?
};

// each node in the graph has one of these, it keeps track of
void ApplyModelSet(const Hypergraph& in,
                   const SentenceMetadata& smeta,
                   const ModelSet& models,
                   const IntersectionConfiguration& config,
                   Hypergraph* out) {
  //force exhaustive if there's no state req. for model
  if (models.stateless() || config.algorithm == IntersectionConfiguration::FULL) {
    NoPruningRescorer ma(models, smeta, in, out); // avoid overhead of best-first when no state
    ma.Apply();
  } else if (config.algorithm == IntersectionConfiguration::CUBE 
             || config.algorithm == IntersectionConfiguration::FAST_CUBE_PRUNING
             || config.algorithm == IntersectionConfiguration::FAST_CUBE_PRUNING_2) {
    int pl = config.pop_limit;
    const int max_pl_for_large=50;
    if (pl > max_pl_for_large && in.nodes_.size() > 80000) {
      pl = max_pl_for_large;
      cerr << "  Note: reducing pop_limit to " << pl << " for very large forest\n";
    }
    if      (config.algorithm == IntersectionConfiguration::CUBE) {
    	CubePruningRescorer ma(models, smeta, in, pl, out);
        ma.Apply();
    }
    else if (config.algorithm == IntersectionConfiguration::FAST_CUBE_PRUNING){
    	CubePruningRescorer ma(models, smeta, in, pl, out, FAST_CP);
        ma.Apply();
    }
    else if (config.algorithm == IntersectionConfiguration::FAST_CUBE_PRUNING_2){
    	CubePruningRescorer ma(models, smeta, in, pl, out, FAST_CP_2);
        ma.Apply();
    }

  } else {
    cerr << "Don't understand intersection algorithm " << config.algorithm << endl;
    exit(1);
  }
  out->is_linear_chain_ = in.is_linear_chain_;  // TODO remove when this is computed
                                                // automatically
}

