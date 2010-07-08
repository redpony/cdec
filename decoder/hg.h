#ifndef _HG_H_
#define _HG_H_

#include <string>
#include <vector>

#include "small_vector.h"
#include "sparse_vector.h"
#include "wordid.h"
#include "trule.h"
#include "prob.h"

// if you define this, edges_ will be sorted
// (normally, just nodes_ are), but this can be quite
// slow
#undef HG_EDGES_TOPO_SORTED

// class representing an acyclic hypergraph
//  - edges have 1 head, 0..n tails
class Hypergraph {
 public:
  Hypergraph() : is_linear_chain_(false) {}

  // SmallVector is a fast, small vector<int> implementation for sizes <= 2
  typedef SmallVector TailNodeVector;

  // TODO get rid of cat_?
  struct Node {
    Node() : id_(), cat_() {}
    int id_; // equal to this object's position in the nodes_ vector
    WordID cat_;  // non-terminal category if <0, 0 if not set
    std::vector<int> in_edges_;   // contents refer to positions in edges_
    std::vector<int> out_edges_;  // contents refer to positions in edges_
  };

  // TODO get rid of edge_prob_? (can be computed on the fly as the dot
  // product of the weight vector and the feature values)
  struct Edge {
    Edge() : i_(-1), j_(-1), prev_i_(-1), prev_j_(-1) {}
    inline int Arity() const { return tail_nodes_.size(); }
    int head_node_;               // refers to a position in nodes_
    TailNodeVector tail_nodes_;   // contents refer to positions in nodes_
    TRulePtr rule_;
    SparseVector<double> feature_values_;
    prob_t edge_prob_;             // dot product of weights and feat_values
    int id_;   // equal to this object's position in the edges_ vector

    // span info. typically, i_ and j_ refer to indices in the source sentence
    // if a synchronous parse has been executed i_ and j_ will refer to indices
    // in the target sentence / lattice and prev_i_ prev_j_ will refer to
    // positions in the source.  Note: it is up to the translator implementation
    // to properly set these values.  For some models (like the Forest-input
    // phrase based model) it may not be straightforward to do.  if these values
    // are not properly set, most things will work but alignment and any features
    // that depend on them will be broken.
    short int i_;
    short int j_;
    short int prev_i_;
    short int prev_j_;
  };

  // returns edge with rule_.IsGoal, returns 0 if none found.  otherwise gives best edge_prob_ - note: I don't think edge_prob_ is viterbi cumulative, so this would just be the best local probability.
  Edge const* ViterbiGoalEdge() const;

  void swap(Hypergraph& other) {
    other.nodes_.swap(nodes_);
    std::swap(is_linear_chain_, other.is_linear_chain_);
    other.edges_.swap(edges_);
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

  Edge* AddEdge(const TRulePtr& rule, const TailNodeVector& tail) {
    edges_.push_back(Edge());
    Edge* edge = &edges_.back();
    edge->rule_ = rule;
    edge->tail_nodes_ = tail;
    edge->id_ = edges_.size() - 1;
    for (int i = 0; i < edge->tail_nodes_.size(); ++i)
      nodes_[edge->tail_nodes_[i]].out_edges_.push_back(edge->id_);
    return edge;
  }

  Node* AddNode(const WordID& cat) {
    nodes_.push_back(Node());
    nodes_.back().cat_ = cat;
    nodes_.back().id_ = nodes_.size() - 1;
    return &nodes_.back();
  }

  void ConnectEdgeToHeadNode(const int edge_id, const int head_id) {
    edges_[edge_id].head_node_ = head_id;
    nodes_[head_id].in_edges_.push_back(edge_id);
  }

  // TODO remove this - use the version that takes indices
  void ConnectEdgeToHeadNode(Edge* edge, Node* head) {
    edge->head_node_ = head->id_;
    head->in_edges_.push_back(edge->id_);
  }

  // merge the goal node from other with this goal node
  void Union(const Hypergraph& other);

  void PrintGraphviz() const;

  // compute the total number of paths in the forest
  double NumberOfPaths() const;

  // BEWARE. this assumes that the source and target language
  // strings are identical and that there are no loops.
  // It assumes a bunch of other things about where the
  // epsilons will be.  It tries to assert failure if you
  // break these assumptions, but it may not.
  // TODO - make this work
  void EpsilonRemove(WordID eps);

  // multiple the weights vector by the edge feature vector
  // (inner product) to set the edge probabilities
  template <typename V>
  void Reweight(const V& weights) {
    for (int i = 0; i < edges_.size(); ++i) {
      Edge& e = edges_[i];
      e.edge_prob_.logeq(e.feature_values_.dot(weights));
    }
  }

  typedef std::vector<prob_t> EdgeProbs;
  typedef std::vector<bool> EdgeMask;

  // computes inside and outside scores for each
  // edge in the hypergraph
  // alpha->size = edges_.size = beta->size
  // returns inside prob of goal node
  prob_t ComputeEdgePosteriors(double scale,
                               EdgeProbs* posts) const;

  // find the score of the very best path passing through each edge
  prob_t ComputeBestPathThroughEdges(EdgeProbs* posts) const;

  // create a new hypergraph consisting only of the nodes / edges
  // in the Viterbi derivation of this hypergraph
  // if edges is set, use the EdgeSelectEdgeWeightFunction
  Hypergraph* CreateViterbiHypergraph(const EdgeMask* edges = NULL) const;

  // move weights as near to the source as possible, resulting in a
  // stochastic automaton.  ONLY FUNCTIONAL FOR *LATTICES*.
  // See M. Mohri and M. Riley. A Weight Pushing Algorithm for Large
  //   Vocabulary Speech Recognition. 2001.
  // the log semiring (NOT tropical) is used
  void PushWeightsToSource(double scale = 1.0);
  // same, except weights are pushed to the goal, works for HGs,
  // not just lattices
  void PushWeightsToGoal(double scale = 1.0);

  void SortInEdgesByEdgeWeights();

  void PruneUnreachable(int goal_node_id); // DEPRECATED

  void RemoveNoncoaccessibleStates(int goal_node_id = -1);

  // remove edges from the hypergraph if prune_edge[edge_id] is true
  // note: if run_inside_algorithm is false, then consumers may be unhappy if you pruned nodes that are built on by nodes that are kept.
  void PruneEdges(const EdgeMask& prune_edge, bool run_inside_algorithm = false);

  // for density>=1.0, keep this many times the edges needed for the 1best derivation
  // if you don't know, use_sum_prod_semiring should be false
  void DensityPruneInsideOutside(const double scale, const bool use_sum_prod_semiring, const double density,const EdgeMask* preserve_mask = NULL,bool safe_inside=false);

  /// drop edge i if edge_margin[i] < prune_below, unless preserve_mask[i]
  void MarginPrune(EdgeProbs const& edge_margin,prob_t prune_below,EdgeMask const* preserve_mask=0,bool safe_inside=false,bool verbose=false);
  // safe_inside: if true, a theoretically redundant (but practically important .001% of the time due to rounding error) inside pruning pass will happen after max-marginal pruning.  if you don't do this, it's possible that the pruned hypergraph will contain outside-reachable (but not inside-buildable) nodes.  that is, a parent will be kept whose children were pruned.  if output, those forests may confuse (crash) e.g. mr_vest_map.  however, if the hyperedges occur in defined-before-use (all edges with head h occur before h is used as a tail) order, then a grace margin for keeping edges that starts leniently and becomes more forbidding will make it impossible for this to occur, i.e. safe_inside=true is not needed.

  // prunes any edge whose score on the best path taking that edge is more than alpha away
  // from the score of the global best past (or the highest edge posterior)
  void BeamPruneInsideOutside(const double scale, const bool use_sum_prod_semiring, const double alpha,const EdgeMask* preserve_mask = NULL,bool safe_inside=false);

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
  std::vector<Node> nodes_;
  // edges_ is not guaranteed to be in any particular order
  std::vector<Edge> edges_;

  // reorder nodes_ so they are in topological order
  // source nodes at 0 sink nodes at size-1
  void TopologicallySortNodesAndEdges(int goal_idx,
                                      const EdgeMask* prune_edges = NULL);
 private:
  Hypergraph(int num_nodes, int num_edges, bool is_lc) : is_linear_chain_(is_lc), nodes_(num_nodes), edges_(num_edges) {}

  static TRulePtr kEPSRule;
  static TRulePtr kUnaryRule;
};

// common WeightFunctions, map an edge -> WeightType
// for generic Viterbi/Inside algorithms
struct EdgeProb {
  typedef prob_t Weight;
  inline const prob_t& operator()(const Hypergraph::Edge& e) const { return e.edge_prob_; }
};

struct EdgeSelectEdgeWeightFunction {
  typedef prob_t Weight;
  typedef std::vector<bool> EdgeMask;
  EdgeSelectEdgeWeightFunction(const EdgeMask& v) : v_(v) {}
  inline prob_t operator()(const Hypergraph::Edge& e) const {
    if (v_[e.id_]) return prob_t::One();
    else return prob_t::Zero();
  }
 private:
  const EdgeMask& v_;
};

struct ScaledEdgeProb {
  ScaledEdgeProb(const double& alpha) : alpha_(alpha) {}
  inline prob_t operator()(const Hypergraph::Edge& e) const { return e.edge_prob_.pow(alpha_); }
  const double alpha_;
  typedef prob_t Weight;
};

// see Li (2010), Section 3.2.2-- this is 'x_e = p_e*r_e'
struct EdgeFeaturesAndProbWeightFunction {
  typedef const SparseVector<prob_t> Weight;
  inline const SparseVector<prob_t> operator()(const Hypergraph::Edge& e) const {
    SparseVector<prob_t> res;
    for (SparseVector<double>::const_iterator it = e.feature_values_.begin();
         it != e.feature_values_.end(); ++it)
      res.set_value(it->first, prob_t(it->second) * e.edge_prob_);
    return res;
  }
};

struct TransitionCountWeightFunction {
  typedef double Weight;
  inline double operator()(const Hypergraph::Edge& e) const { (void)e; return 1.0; }
};

#endif
