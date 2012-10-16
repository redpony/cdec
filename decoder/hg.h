#ifndef _HG_H_
#define _HG_H_

// define USE_INFO_EDGE 1 if you want lots of debug info shown with --show_derivations - otherwise it adds quite a bit of overhead if ffs have their logging enabled (e.g. ff_from_fsa)
#ifndef USE_INFO_EDGE
# define USE_INFO_EDGE 0
#endif
#if USE_INFO_EDGE
# define INFO_EDGE(e,msg) do { std::ostringstream &o=(e.info_);o<<msg; } while(0)
# define INFO_EDGEw(e,msg) do { std::ostringstream &o(e.info_);if (o.empty()) o<<' ';o<<msg; } while(0)
#else
# define INFO_EDGE(e,msg)
# define INFO_EDGEw(e,msg)
#endif
#define INFO_EDGEln(e,msg) INFO_EDGE(e,msg<<'\n')

#include <sstream>
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

#include "feature_vector.h"
#include "small_vector.h"
#include "wordid.h"
#include "tdict.h"
#include "trule.h"
#include "prob.h"
#include "indices_after.h"
#include "nt_span.h"

// if you define this, edges_ will be sorted
// (normally, just nodes_ are - root must be nodes_.back()), but this can be quite
// slow
#undef HG_EDGES_TOPO_SORTED

// SmallVector is a fast, small vector<int> implementation for sizes <= 2
typedef SmallVectorUnsigned TailNodeVector; // indices in nodes_
typedef std::vector<int> EdgesVector; // indices in edges_

enum {
  NONE=0,CATEGORY=1,SPAN=2,PROB=4,FEATURES=8,RULE=16,RULE_LHS=32,PREV_SPAN=64,ALL=0xFFFFFFFF
};

namespace HG {

  struct Edge {
    Edge() : i_(-1), j_(-1), prev_i_(-1), prev_j_(-1) {}
    Edge(int id,Edge const& copy_pod_from) : id_(id) { copy_pod(copy_pod_from); } // call copy_features yourself later.
    Edge(int id,Edge const& copy_from,TailNodeVector const& tail) // fully inits - probably more expensive when push_back(Edge(...)) than sett
      : tail_nodes_(tail),id_(id) { copy_pod(copy_from);copy_features(copy_from); }
    inline int Arity() const { return tail_nodes_.size(); }
    int head_node_;               // refers to a position in nodes_
    TailNodeVector tail_nodes_;   // contents refer to positions in nodes_
    TRulePtr rule_;
    SparseVector<weight_t> feature_values_;
    prob_t edge_prob_;             // dot product of weights and feat_values
    int id_;   // equal to this object's position in the edges_ vector

    // span info. typically, i_ and j_ refer to indices in the source sentence.
    // In synchronous parsing, i_ and j_ will refer to target sentence/lattice indices
    // while prev_i_ prev_j_ will refer to positions in the source.
    // Note: it is up to the translator implementation
    // to properly set these values.  For some models (like the Forest-input
    // phrase based model) it may not be straightforward to do.  if these values
    // are not properly set, most things will work but alignment and any features
    // that depend on them will be broken.
    short int i_;
    short int j_;
    short int prev_i_;
    short int prev_j_;
    void show(std::ostream &o,unsigned mask=SPAN|RULE) const {
      o<<'{';
      if (mask&CATEGORY)
        o<< '[' << TD::Convert(-rule_->GetLHS()) << ']';
      if (mask&PREV_SPAN)
        o<<'<'<<prev_i_<<','<<prev_j_<<'>';
      if (mask&SPAN)
        o<<'<'<<i_<<','<<j_<<'>';
      if (mask&PROB)
        o<<" p="<<edge_prob_;
      if (mask&FEATURES)
        o<<' '<<feature_values_;
      if (mask&RULE)
        o<<' '<<rule_->AsString(mask&RULE_LHS);
      o<<'}';
    }
    std::string show(unsigned mask=SPAN|RULE) const {
      std::ostringstream o;
      show(o,mask);
      return o.str();
    }
    void copy_pod(Edge const& o) {
      rule_=o.rule_;
      i_ = o.i_; j_ = o.j_; prev_i_ = o.prev_i_; prev_j_ = o.prev_j_;
    }
    void copy_features(Edge const& o) {
      feature_values_=o.feature_values_;
    }
    void copy_fixed(Edge const& o) {
      copy_pod(o);
      copy_features(o);
      edge_prob_ = o.edge_prob_;
    }
    void copy_reindex(Edge const& o,indices_after const& n2,indices_after const& e2) {
      copy_fixed(o);
      head_node_=n2[o.head_node_];
      id_=e2[o.id_];
      n2.reindex_push_back(o.tail_nodes_,tail_nodes_);
    }
    // generic recursion re: child_handle=re(tail_nodes_[i],i,parent_handle)
    //   FIXME: make kbest create a simple derivation-tree structure (could be a
    //   hg), and replace the list-of-edges viterbi.h with a tree-structured one.
    //   CreateViterbiHypergraph can do for 1best, though.
    template <class EdgeRecurse,class TEdgeHandle>
    std::string derivation_tree(EdgeRecurse const& re,TEdgeHandle const& eh,bool indent=true,int show_mask=SPAN|RULE,int maxdepth=0x7FFFFFFF,int depth=0) const {
      std::ostringstream o;
      derivation_tree_stream(re,eh,o,indent,show_mask,maxdepth,depth);
      return o.str();
    }
    template <class EdgeRecurse,class TEdgeHandle>
    void derivation_tree_stream(EdgeRecurse const& re,TEdgeHandle const& eh,std::ostream &o,bool indent=true,int show_mask=SPAN|RULE,int maxdepth=0x7FFFFFFF,int depth=0) const {
      if (depth>maxdepth) return;
      if (indent) for (int i=0;i<depth;++i) o<<' ';
      o<<'(';
      show(o,show_mask);
      if (indent) o<<'\n';
      for (unsigned i=0;i<tail_nodes_.size();++i) {
        TEdgeHandle c=re(tail_nodes_[i],i,eh);
        Edge const* cp=c;
        if (cp) {
          cp->derivation_tree_stream(re,c,o,indent,show_mask,maxdepth,depth+1);
          if (!indent) o<<' ';
        }
      }
      if (indent) for (int i=0;i<depth;++i) o<<' ';
      o<<")";
      if (indent) o<<"\n";
    }
  };

  // TODO get rid of cat_?
  // TODO keep cat_ and add span and/or state? :)
  struct Node {
    Node() : id_(), cat_() {}
    int id_; // equal to this object's position in the nodes_ vector
    WordID cat_;  // non-terminal category if <0, 0 if not set
    WordID NT() const { return -cat_; }
    EdgesVector in_edges_;   // an in edge is an edge with this node as its head.  (in edges come from the bottom up to us)  indices in edges_
    EdgesVector out_edges_;  // an out edge is an edge with this node as its tail.  (out edges leave us up toward the top/goal). indices in edges_
    void copy_fixed(Node const& o) { // nonstructural fields only - structural ones are managed by sorting/pruning/subsetting
      cat_=o.cat_;
    }
    void copy_reindex(Node const& o,indices_after const& n2,indices_after const& e2) {
      copy_fixed(o);
      id_=n2[id_];
      e2.reindex_push_back(o.in_edges_,in_edges_);
      e2.reindex_push_back(o.out_edges_,out_edges_);
    }
  };

} // namespace HG

class Hypergraph;
typedef boost::shared_ptr<Hypergraph> HypergraphP;
// class representing an acyclic hypergraph
//  - edges have 1 head, 0..n tails
class Hypergraph {
public:
  Hypergraph() : is_linear_chain_(false) {}
  typedef HG::Node Node;
  typedef HG::Edge Edge;
  typedef SmallVectorUnsigned TailNodeVector; // indices in nodes_
  typedef std::vector<int> EdgesVector; // indices in edges_
  enum {
    NONE=0,CATEGORY=1,SPAN=2,PROB=4,FEATURES=8,RULE=16,RULE_LHS=32,PREV_SPAN=64,ALL=0xFFFFFFFF
  };

  // except for stateful models that have split nt,span, this should identify the node
  void SetNodeOrigin(int nodeid,NTSpan &r) const {
    Node const &n=nodes_[nodeid];
    r.nt=n.NT();
    if (!n.in_edges_.empty()) {
      Edge const& e=edges_[n.in_edges_.front()];
      r.s.l=e.i_;
      r.s.r=e.j_;
//      if (e.rule_) r.nt=-e.rule_->lhs_;
    }
  }
  NTSpan NodeOrigin(int nodeid) const {
    NTSpan r;
    SetNodeOrigin(nodeid,r);
    return r;
  }
  Span NodeSpan(int nodeid) const {
    Span s;
    Node const &n=nodes_[nodeid];
    if (!n.in_edges_.empty()) {
      Edge const& e=edges_[n.in_edges_.front()];
      s.l=e.i_;
      s.r=e.j_;
    }
    return s;
  }
  WordID NodeLHS(int nodeid) const {
    Node const &n=nodes_[nodeid];
    return n.NT();
  }

  typedef std::vector<prob_t> EdgeProbs;
  typedef std::vector<prob_t> NodeProbs;
  typedef std::vector<bool> EdgeMask;
  typedef std::vector<bool> NodeMask;

  std::string show_viterbi_tree(bool indent=true,int show_mask=SPAN|RULE,int maxdepth=0x7FFFFFFF,int depth=0) const;

  std::string show_first_tree(bool indent=true,int show_mask=SPAN|RULE,int maxdepth=0x7FFFFFFF,int depth=0) const;

  typedef Edge const* EdgeHandle;
  EdgeHandle operator()(int tailn,int /*taili*/,EdgeHandle /*parent*/) const {
    return first_edge(tailn);
  }

  Edge const* first_edge(int node) const { // only actually viterbi if ViterbiSortInEdges() called.  otherwise it's just the first.
    EdgesVector const& v=nodes_[node].in_edges_;
    return v.empty() ? 0 : &edges_[v.front()];
  }

  Edge const* first_edge() const {
    int nn=nodes_.size();
    return nn>=0?first_edge(nn-1):0;
  }


#if 0
  // returns edge with rule_.IsGoal, returns 0 if none found.  otherwise gives best edge_prob_ - note: I don't think edge_prob_ is viterbi cumulative, so this would just be the best local probability.
  Edge const* ViterbiGoalEdge() const;
#endif

  int GoalNode() const { return nodes_.size()-1; } // by definition, and sorting of nodes in topo order (bottom up)

// post: in_edges_ for each node is ordered by global viterbi.  returns 1best goal node edge as well
  Edge const* ViterbiSortInEdges();
  Edge const* SortInEdgesByNodeViterbi(NodeProbs const& nv);
  Edge const* ViterbiSortInEdges(EdgeProbs const& eviterbi);

  prob_t ComputeNodeViterbi(NodeProbs *np) const;
  prob_t ComputeEdgeViterbi(EdgeProbs *ev) const;
  prob_t ComputeEdgeViterbi(NodeProbs const&np,EdgeProbs *ev) const;

  void swap(Hypergraph& other) {
    other.nodes_.swap(nodes_);
    std::swap(is_linear_chain_, other.is_linear_chain_);
    other.edges_.swap(edges_);
  }
  friend inline void swap(Hypergraph &a,Hypergraph &b) {
    a.swap(b);
  }

  void ResizeNodes(int size) {
    nodes_.resize(size);
    for (int i = 0; i < size; ++i) nodes_[i].id_ = i;
  }

  // reserves space in the nodes vector to prevent memory locations
  // from changing
  void ReserveNodes(size_t n, size_t e = 0) {
    nodes_.reserve(n);
    if (e) edges_.reserve(e);
  }

private:
  void index_tails(Edge const& edge) {
    for (unsigned i = 0; i < edge.tail_nodes_.size(); ++i)
      nodes_[edge.tail_nodes_[i]].out_edges_.push_back(edge.id_);
  }
public:
  // the below AddEdge all are used mostly for apply_models scoring and so do not set prob_ ; also, you will need to ConnectEdgeToHeadNode yourself (since head may be new)

  // tails are already set, copy_fixed members are already set.  all we need to do is set id and add to out_edges of tails
  Edge* AddEdge(Edge const& nedge) {
    int eid=edges_.size();
    edges_.push_back(nedge);
    Edge* edge = &edges_.back();
    edge->id_ = eid;
    index_tails(*edge);
    return edge;
  }

  // also copies feature vector
  Edge* AddEdge(Edge const& in_edge, const TailNodeVector& tail) {
    edges_.push_back(Edge(edges_.size(),in_edge));
    Edge* edge = &edges_.back();
    edge->feature_values_ = in_edge.feature_values_;
    edge->tail_nodes_ = tail; // possibly faster than copying to Edge() constructed above then copying via push_back.  perhaps optimized it's the same.
    index_tails(*edge);
    return edge;
  }

  // oldest method in use - should use in parsing (no models) only, in rescoring requires much manual assignment from source edge; favor the previous instead
  Edge* AddEdge(const TRulePtr& rule, const TailNodeVector& tail) {
    int eid=edges_.size();
    edges_.push_back(Edge());
    Edge* edge = &edges_.back();
    edge->rule_ = rule;
    edge->tail_nodes_ = tail;
    edge->id_ = eid;
    for (unsigned i = 0; i < edge->tail_nodes_.size(); ++i)
      nodes_[edge->tail_nodes_[i]].out_edges_.push_back(edge->id_);
    return edge;
  }

  Node* AddNode(const WordID& cat) {
    nodes_.push_back(Node());
    nodes_.back().cat_ = cat;
    nodes_.back().id_ = nodes_.size() - 1;
    return &nodes_.back();
  }

  //TODO: use indices everywhere?  bottom two are a bit redundant.
  void ConnectEdgeToHeadNode(const int edge_id, const int head_id) {
    edges_[edge_id].head_node_ = head_id;
    nodes_[head_id].in_edges_.push_back(edge_id);
  }

  void ConnectEdgeToHeadNode(Edge* edge, Node* head) {
    edge->head_node_ = head->id_;
    head->in_edges_.push_back(edge->id_);
  }

  void ConnectEdgeToHeadNode(Edge* edge, int head_id) {
    edge->head_node_ = head_id;
    nodes_[head_id].in_edges_.push_back(edge->id_);
  }

  // merge the goal node from other with this goal node
  void Union(const Hypergraph& other);

  void PrintGraphviz() const;

  // compute the total number of paths in the forest
  double NumberOfPaths() const;

  // multiple the weights vector by the edge feature vector
  // (inner product) to set the edge probabilities
  template <class V>
  void Reweight(const V& weights) {
    for (unsigned i = 0; i < edges_.size(); ++i) {
      Edge& e = edges_[i];
      e.edge_prob_.logeq(e.feature_values_.dot(weights));
    }
  }

  // computes inside and outside scores for each
  // edge in the hypergraph
  // alpha->size = edges_.size = beta->size
  // returns inside prob of goal node
  prob_t ComputeEdgePosteriors(double scale,EdgeProbs* posts) const;

  // find the score of the very best path passing through each edge
  prob_t ComputeBestPathThroughEdges(EdgeProbs* posts) const;


  /* for all of the below subsets, the hg Nodes must be topo sorted already*/

  // keep_nodes is an output-only param, keep_edges is in-out (more may be pruned than you asked for if the tails can't be built)
  HypergraphP CreateEdgeSubset(EdgeMask & keep_edges_in_out,NodeMask &keep_nodes_out) const;
  HypergraphP CreateEdgeSubset(EdgeMask & keep_edges) const;
  // node keeping is final (const), edge keeping is an additional constraint which will sometimes be made stricter (and output back to you) by the absence of nodes.  you may end up with some empty nodes.  that is, kept edges will be made consistent with kept nodes first (but empty nodes are allowed)
  HypergraphP CreateNodeSubset(NodeMask const& keep_nodes,EdgeMask &keep_edges_in_out) const {
    TightenEdgeMask(keep_edges_in_out,keep_nodes);
    return CreateNodeEdgeSubset(keep_nodes,keep_edges_in_out);
  }
  HypergraphP CreateNodeSubset(NodeMask const& keep_nodes) const {
    EdgeMask ke(edges_.size(),true);
    return CreateNodeSubset(keep_nodes,ke);
  }

  void TightenEdgeMask(EdgeMask &ke,NodeMask const& kn) const;
  // kept edges are consistent with kept nodes already:
  HypergraphP CreateNodeEdgeSubset(NodeMask const& keep_nodes,EdgeMask const&keep_edges_in_out) const;
  HypergraphP CreateNodeEdgeSubset(NodeMask const& keep_nodes) const;


  // create a new hypergraph consisting only of the nodes / edges
  // in the Viterbi derivation of this hypergraph
  // if edges is set, use the EdgeSelectEdgeWeightFunction
  // NOTE: last edge/node index are goal
  HypergraphP CreateViterbiHypergraph(const EdgeMask* edges = NULL) const;

  // move weights as near to the source as possible, resulting in a
  // stochastic automaton.  ONLY FUNCTIONAL FOR *LATTICES*.
  // See M. Mohri and M. Riley. A Weight Pushing Algorithm for Large
  //   Vocabulary Speech Recognition. 2001.
  // the log semiring (NOT tropical) is used
  void PushWeightsToSource(double scale = 1.0);
  // same, except weights are pushed to the goal, works for HGs,
  // not just lattices
  prob_t PushWeightsToGoal(double scale = 1.0);

  // contrary to PushWeightsToGoal, use viterbi semiring; store log(p) to fid.  note that p_viterbi becomes 1; k*p_viterbi becomes k.  also modifies edge_prob_ (note that the fid stored log(p) will stick around even if you reweight)
  // afterwards, product of edge_prob_ for a derivation will equal 1 for the viterbi (p_v before, 1 after), and in general (k*p_v before, k after).  returns inside(goal)
  prob_t PushViterbiWeightsToGoal(int fid=0);

//  void SortInEdgesByEdgeWeights(); // local sort only - pretty useless

  void PruneUnreachable(int goal_node_id); // DEPRECATED

  // remove edges from the hypergraph if prune_edge[edge_id] is true
  // note: if run_inside_algorithm is false, then consumers may be unhappy if you pruned nodes that are built on by nodes that are kept.
  void PruneEdges(const EdgeMask& prune_edge, bool run_inside_algorithm = false);

  /// drop edge i if edge_margin[i] < prune_below, unless preserve_mask[i]
  void MarginPrune(EdgeProbs const& edge_margin,prob_t prune_below,EdgeMask const* preserve_mask=0,bool safe_inside=false,bool verbose=false);

  //TODO: in my opinion, looking at the ratio of logprobs (features \dot weights) rather than the absolute difference generalizes more nicely across sentence lengths and weight vectors that are constant multiples of one another.  at least make that an option.  i worked around this a little in cdec by making "beam alpha per source word" but that's not helping with different tuning runs.

  // beam_alpha=0 means don't beam prune, otherwise drop things that are e^beam_alpha times worse than best -   // prunes any edge whose prob_t on the best path taking that edge is more than e^alpha times
  //density=0 means don't density prune:   // for density>=1.0, keep this many times the edges needed for the 1best derivation
  // worse than the score of the global best past (or the highest edge posterior)
  // scale is for use_sum_prod_semiring (sharpens distribution?)
  // returns true if density pruning was tighter than beam
  // safe_inside would be a redundant anti-rounding error second bottom-up reachability before actually removing edges, to prevent stranded edges.  shouldn't be needed - if the hyperedges occur in defined-before-use (all edges with head h occur before h is used as a tail) order, then a grace margin for keeping edges that starts leniently and becomes more forbidding will make it impossible for this to occur, i.e. safe_inside=true is not needed.
  bool PruneInsideOutside(double beam_alpha,double density,const EdgeMask* preserve_mask = NULL,const bool use_sum_prod_semiring=false, const double scale=1,bool safe_inside=false);

  // legacy:
  void DensityPruneInsideOutside(const double scale, const bool use_sum_prod_semiring, const double density,const EdgeMask* preserve_mask = NULL) {
    PruneInsideOutside(0,density,preserve_mask,use_sum_prod_semiring,scale);
  }

  // legacy:
  void BeamPruneInsideOutside(const double scale, const bool use_sum_prod_semiring, const double alpha,const EdgeMask* preserve_mask = NULL) {
    PruneInsideOutside(alpha,0,preserve_mask,use_sum_prod_semiring,scale);
  }

  // report nodes, edges, paths
  std::string stats(std::string const& name="forest") const;

  void clear() {
    nodes_.clear();
    edges_.clear();
  }

  inline size_t NumberOfEdges() const { return edges_.size(); }
  inline size_t NumberOfNodes() const { return nodes_.size(); }
  inline bool empty() const { return nodes_.empty(); }

  // linear chains can be represented in a number of ways in a hypergraph,
  // we define them to consist only of lexical translations and monotonic rules
  inline bool IsLinearChain() const { return is_linear_chain_; }
  bool is_linear_chain_;

  // nodes_ is sorted in topological order
  typedef std::vector<Node> Nodes;
  Nodes nodes_;
  // edges_ is not guaranteed to be in any particular order
  typedef std::vector<Edge> Edges;
  Edges edges_;
  bool edges_topo_; // TODO: this is always true right now - should reflect whether edges_ are ordered.  typically, you can just iterate over nodes (which are in topo order) and use in_edges to get the edges in topo order

  template <class V>
  void visit_edges_topo(V &v) {
    for (unsigned i = 0; i < nodes_.size(); ++i) {
      EdgesVector const& in=nodes_[i].in_edges_;
      for (unsigned j=0;j<in.size();++j) {
        int e=in[j];
        v(i,e,edges_[e]);
      }
    }
  }

  template <class V>
  void visit_edges(V &v) {
    for (unsigned i=0;i<edges_.size();++i)
      v(edges_[i].head_node_,i,edges_[i]);
  }

  // reorder nodes_ so they are in topological order
  // source nodes at 0 sink nodes at size-1
  void TopologicallySortNodesAndEdges(int goal_idx, const EdgeMask* prune_edges = NULL);

  void set_ids(); // resync edge,node .id_
  void check_ids() const; // assert that .id_ have been kept in sync

private:
  Hypergraph(int num_nodes, int num_edges, bool is_lc) : is_linear_chain_(is_lc), nodes_(num_nodes), edges_(num_edges),edges_topo_(true) {}
};


// common WeightFunctions, map an edge -> WeightType
// for generic Viterbi/Inside algorithms
struct EdgeProb {
  typedef prob_t Weight;
  inline const prob_t& operator()(const HG::Edge& e) const { return e.edge_prob_; }
};

struct EdgeSelectEdgeWeightFunction {
  typedef prob_t Weight;
  typedef std::vector<bool> EdgeMask;
  EdgeSelectEdgeWeightFunction(const EdgeMask& v) : v_(v) {}
  inline prob_t operator()(const HG::Edge& e) const {
    if (v_[e.id_]) return prob_t::One();
    else return prob_t::Zero();
  }
private:
  const EdgeMask& v_;
};

struct ScaledEdgeProb {
  ScaledEdgeProb(const double& alpha) : alpha_(alpha) {}
  inline prob_t operator()(const HG::Edge& e) const { return e.edge_prob_.pow(alpha_); }
  const double alpha_;
  typedef prob_t Weight;
};

// see Li (2010), Section 3.2.2-- this is 'x_e = p_e*r_e'
struct EdgeFeaturesAndProbWeightFunction {
  typedef SparseVector<prob_t> Weight;
  typedef Weight Result; //TODO: change Result->Weight everywhere?
  inline const Weight operator()(const HG::Edge& e) const {
    SparseVector<prob_t> res;
    for (SparseVector<double>::const_iterator it = e.feature_values_.begin();
         it != e.feature_values_.end(); ++it)
      res.set_value(it->first, prob_t(it->second) * e.edge_prob_);
    return res;
  }
};

struct TransitionCountWeightFunction {
  typedef double Weight;
  inline double operator()(const HG::Edge& e) const { (void)e; return 1.0; }
};

#endif
