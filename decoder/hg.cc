#include "hg.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <set>
#include <map>
#include <iostream>

#include "viterbi.h"
#include "inside_outside.h"
#include "tdict.h"

using namespace std;

double Hypergraph::NumberOfPaths() const {
  return Inside<double, TransitionCountWeightFunction>(*this);
}

struct ScaledTransitionEventWeightFunction {
  ScaledTransitionEventWeightFunction(double alpha) : scale_(alpha) {}
  inline SparseVector<prob_t> operator()(const Hypergraph::Edge& e) const {
    SparseVector<prob_t> result;
    result.set_value(e.id_, e.edge_prob_.pow(scale_));
    return result;
  }
  const double scale_;
};

struct TropicalValue {
  TropicalValue() : v_() {}
  explicit TropicalValue(int v) {
    if (v == 0) v_ = prob_t::Zero();
    else if (v == 1) v_ = prob_t::One();
    else { cerr << "Bad value in TropicalValue(int).\n"; abort(); }
  }
  explicit TropicalValue(const prob_t& v) : v_(v) {}
  inline TropicalValue& operator+=(const TropicalValue& o) {
    if (v_ < o.v_) v_ = o.v_;
    return *this;
  }
  inline TropicalValue& operator*=(const TropicalValue& o) {
    v_ *= o.v_;
    return *this;
  }
  inline bool operator==(const TropicalValue& o) const { return v_ == o.v_; }
  prob_t v_;
};

struct ViterbiWeightFunction {
  inline TropicalValue operator()(const Hypergraph::Edge& e) const {
    return TropicalValue(e.edge_prob_);
  }
};

struct ViterbiTransitionEventWeightFunction {
  inline SparseVector<TropicalValue> operator()(const Hypergraph::Edge& e) const {
    SparseVector<TropicalValue> result;
    result.set_value(e.id_, TropicalValue(e.edge_prob_));
    return result;
  }
};


prob_t Hypergraph::ComputeEdgePosteriors(double scale, vector<prob_t>* posts) const {
  const ScaledEdgeProb weight(scale);
  const ScaledTransitionEventWeightFunction w2(scale);
  SparseVector<prob_t> pv;
  const double inside = InsideOutside<prob_t,
                  ScaledEdgeProb,
                  SparseVector<prob_t>,
                  ScaledTransitionEventWeightFunction>(*this, &pv, weight, w2);
  posts->resize(edges_.size());
  for (int i = 0; i < edges_.size(); ++i)
    (*posts)[i] = prob_t(pv.value(i));
  return prob_t(inside);
}

prob_t Hypergraph::ComputeBestPathThroughEdges(vector<prob_t>* post) const {
  SparseVector<TropicalValue> pv;
  const TropicalValue viterbi_weight = InsideOutside<TropicalValue,
                  ViterbiWeightFunction,
                  SparseVector<TropicalValue>,
                  ViterbiTransitionEventWeightFunction>(*this, &pv);
  post->resize(edges_.size());
  for (int i = 0; i < edges_.size(); ++i)
    (*post)[i] = pv.value(i).v_;
  return viterbi_weight.v_;
}

void Hypergraph::PushWeightsToSource(double scale) {
  vector<prob_t> posts;
  ComputeEdgePosteriors(scale, &posts);
  for (int i = 0; i < nodes_.size(); ++i) {
    const Hypergraph::Node& node = nodes_[i];
    prob_t z = prob_t::Zero();
    for (int j = 0; j < node.out_edges_.size(); ++j)
      z += posts[node.out_edges_[j]];
    for (int j = 0; j < node.out_edges_.size(); ++j) {
      edges_[node.out_edges_[j]].edge_prob_ = posts[node.out_edges_[j]] / z;
    }
  }
}

void Hypergraph::PushWeightsToGoal(double scale) {
  vector<prob_t> posts;
  ComputeEdgePosteriors(scale, &posts);
  for (int i = 0; i < nodes_.size(); ++i) {
    const Hypergraph::Node& node = nodes_[i];
    prob_t z = prob_t::Zero();
    for (int j = 0; j < node.in_edges_.size(); ++j)
      z += posts[node.in_edges_[j]];
    for (int j = 0; j < node.in_edges_.size(); ++j) {
      edges_[node.in_edges_[j]].edge_prob_ = posts[node.in_edges_[j]] / z;
    }
  }
}

struct EdgeExistsWeightFunction {
  EdgeExistsWeightFunction(const std::vector<bool>& prunes) : prunes_(prunes) {}
  double operator()(const Hypergraph::Edge& edge) const {
    return (prunes_[edge.id_] ? 0.0 : 1.0);
  }
 private:
  const vector<bool>& prunes_;
};

void Hypergraph::PruneEdges(const std::vector<bool>& prune_edge, bool run_inside_algorithm) {
  assert(prune_edge.size() == edges_.size());
  vector<bool> filtered = prune_edge;

  if (run_inside_algorithm) {
    const EdgeExistsWeightFunction wf(prune_edge);
    // use double, not bool since vector<bool> causes problems with the Inside algorithm.
    // I don't know a good c++ way to resolve this short of template specialization which
    // I dislike.  If you know of a better way that doesn't involve specialization,
    // fix this!
    vector<double> reachable;
    bool goal_derivable = (0 < Inside<double, EdgeExistsWeightFunction>(*this, &reachable, wf));
    if (!goal_derivable) {
      edges_.clear();
      nodes_.clear();
      nodes_.push_back(Node());
      return;
    }

    assert(reachable.size() == nodes_.size());
    for (int i = 0; i < edges_.size(); ++i) {
      bool prune = prune_edge[i];
      if (!prune) {
        const Edge& edge = edges_[i];
        for (int j = 0; j < edge.tail_nodes_.size(); ++j) {
          if (!reachable[edge.tail_nodes_[j]]) {
            prune = true;
            break;
          }
        }
      }
      filtered[i] = prune;
    }
  }
 
  TopologicallySortNodesAndEdges(nodes_.size() - 1, &filtered);
}

void Hypergraph::DensityPruneInsideOutside(const double scale,
                                           const bool use_sum_prod_semiring,
                                           const double density,
                                           const vector<bool>* preserve_mask) {
  assert(density >= 1.0);
  const int plen = ViterbiPathLength(*this);
  vector<WordID> bp;
  int rnum = min(static_cast<int>(edges_.size()), static_cast<int>(density * static_cast<double>(plen)));
  if (rnum == edges_.size()) {
    cerr << "No pruning required: denisty already sufficient";
    return;
  }
  vector<prob_t> io(edges_.size());
  if (use_sum_prod_semiring)
    ComputeEdgePosteriors(scale, &io);
  else
    ComputeBestPathThroughEdges(&io);
  assert(edges_.size() == io.size());
  vector<prob_t> sorted = io;
  nth_element(sorted.begin(), sorted.begin() + rnum, sorted.end(), greater<prob_t>());
  const double cutoff = sorted[rnum];
  vector<bool> prune(edges_.size());
  for (int i = 0; i < edges_.size(); ++i) {
    prune[i] = (io[i] < cutoff);
    if (preserve_mask && (*preserve_mask)[i]) prune[i] = false;
  }
  PruneEdges(prune);
}

void Hypergraph::BeamPruneInsideOutside(
    const double scale,
    const bool use_sum_prod_semiring,
    const double alpha,
    const vector<bool>* preserve_mask) {
  assert(alpha > 0.0);
  assert(scale > 0.0);
  vector<prob_t> io(edges_.size());
  if (use_sum_prod_semiring)
    ComputeEdgePosteriors(scale, &io);
  else
    ComputeBestPathThroughEdges(&io);
  assert(edges_.size() == io.size());
  prob_t best;  // initializes to zero
  for (int i = 0; i < io.size(); ++i)
    if (io[i] > best) best = io[i];
  const prob_t aprob(exp(-alpha));
  const prob_t cutoff = best * aprob;
  // cerr << "aprob = " << aprob << "\t  CUTOFF=" << cutoff << endl;
  vector<bool> prune(edges_.size());
  //cerr << preserve_mask.size() << " " << edges_.size() << endl;
  int pc = 0;
  for (int i = 0; i < io.size(); ++i) {
    const bool prune_edge = (io[i] < cutoff);
    if (prune_edge) ++pc;
    prune[i] = (io[i] < cutoff);
    if (preserve_mask && (*preserve_mask)[i]) prune[i] = false;
  }
  // cerr << "Beam pruning " << pc << "/" << io.size() << " edges\n";
  PruneEdges(prune);
}

void Hypergraph::PrintGraphviz() const {
  int ei = 0;
  cerr << "digraph G {\n  rankdir=LR;\n  nodesep=.05;\n";
  for (vector<Edge>::const_iterator i = edges_.begin();
       i != edges_.end(); ++i) {
    const Edge& edge=*i;
    ++ei;
    static const string none = "<null>";
    string rule = (edge.rule_ ? edge.rule_->AsString(false) : none);

    cerr << "   A_" << ei << " [label=\"" << rule << " p=" << edge.edge_prob_ 
         << " F:" << edge.feature_values_
         << "\" shape=\"rect\"];\n";
    Hypergraph::TailNodeVector indorder(edge.tail_nodes_.size(), 0);
    int ntc = 0;
    for (int i = 0; i < edge.rule_->e_.size(); ++i) {
      if (edge.rule_->e_[i] <= 0) indorder[ntc++] = 1 + (-1 * edge.rule_->e_[i]);
    }
    for (int i = 0; i < edge.tail_nodes_.size(); ++i) {
      cerr << "     " << edge.tail_nodes_[i] << " -> A_" << ei;
      if (edge.tail_nodes_.size() > 1) {
        cerr << " [label=\"" << indorder[i] << "\"]";
      }
      cerr << ";\n";
    }
    cerr << "     A_" << ei << " -> " << edge.head_node_ << ";\n";
  }
  for (vector<Node>::const_iterator ni = nodes_.begin();
      ni != nodes_.end(); ++ni) {
    cerr << "  " << ni->id_ << "[label=\"" << (ni->cat_ < 0 ? TD::Convert(ni->cat_ * -1) : "")
    //cerr << "  " << ni->id_ << "[label=\"" << ni->cat_
         << " n=" << ni->id_
//         << ",x=" << &*ni
//         << ",in=" << ni->in_edges_.size() 
//         << ",out=" << ni->out_edges_.size()
         << "\"];\n";
  }
  cerr << "}\n";
}

void Hypergraph::Union(const Hypergraph& other) {
  if (&other == this) return;
  if (nodes_.empty()) { nodes_ = other.nodes_; edges_ = other.edges_; return; }
  int noff = nodes_.size();
  int eoff = edges_.size();
  int ogoal = other.nodes_.size() - 1;
  int cgoal = noff - 1;
  // keep a single goal node, so add nodes.size - 1
  nodes_.resize(nodes_.size() + ogoal);
  // add all edges
  edges_.resize(edges_.size() + other.edges_.size());

  for (int i = 0; i < ogoal; ++i) {
    const Node& on = other.nodes_[i];
    Node& cn = nodes_[i + noff];
    cn.id_ = i + noff;
    cn.in_edges_.resize(on.in_edges_.size());
    for (int j = 0; j < on.in_edges_.size(); ++j)
      cn.in_edges_[j] = on.in_edges_[j] + eoff;

    cn.out_edges_.resize(on.out_edges_.size());
    for (int j = 0; j < on.out_edges_.size(); ++j)
      cn.out_edges_[j] = on.out_edges_[j] + eoff;
  }

  for (int i = 0; i < other.edges_.size(); ++i) {
    const Edge& oe = other.edges_[i];
    Edge& ce = edges_[i + eoff];
    ce.id_ = i + eoff;
    ce.rule_ = oe.rule_;
    ce.feature_values_ = oe.feature_values_;
    if (oe.head_node_ == ogoal) {
      ce.head_node_ = cgoal;
      nodes_[cgoal].in_edges_.push_back(ce.id_);
    } else {
      ce.head_node_ = oe.head_node_ + noff;
    }
    ce.tail_nodes_.resize(oe.tail_nodes_.size());
    for (int j = 0; j < oe.tail_nodes_.size(); ++j)
      ce.tail_nodes_[j] = oe.tail_nodes_[j] + noff;
  }

  TopologicallySortNodesAndEdges(cgoal);
}

void Hypergraph::PruneUnreachable(int goal_node_id) {
  TopologicallySortNodesAndEdges(goal_node_id, NULL);
}

void Hypergraph::RemoveNoncoaccessibleStates(int goal_node_id) {
  if (goal_node_id < 0) goal_node_id += nodes_.size();
  assert(goal_node_id >= 0);
  assert(goal_node_id < nodes_.size());

  // TODO finish implementation
  abort();
}

struct DFSContext {
  int node;
  int edge_iter;
  int tail_iter;
  DFSContext(int n, int e, int t) : node(n), edge_iter(e), tail_iter(t) {}
};

enum ColorType { WHITE, GRAY, BLACK };

template <class T>
struct BadId {
  bool operator()(const T& obj) const { return obj.id_ == -1; }
};

template <class T>
struct IdCompare {
  bool operator()(const T& a, const T& b) { return a.id_ < b.id_; }
};

void Hypergraph::TopologicallySortNodesAndEdges(int goal_index,
                                                const vector<bool>* prune_edges) {
  // figure out which nodes are reachable from the goal
  vector<int> reloc_node(nodes_.size(), -1);
  vector<int> reloc_edge(edges_.size(), -1);
  vector<ColorType> color(nodes_.size(), WHITE);
  vector<DFSContext> stack;
  stack.reserve(nodes_.size());
  stack.push_back(DFSContext(goal_index, 0, 0));
  int node_count = 0;
  int edge_count = 0;
  while(!stack.empty()) {
    const DFSContext& p = stack.back();
    int cur_ni = p.node;
    int edge_i = p.edge_iter;
    int tail_i = p.tail_iter;
    stack.pop_back();
    const Node* cur_node = &nodes_[cur_ni];
    int edge_end = cur_node->in_edges_.size();
    while (edge_i != edge_end) {
      const Edge& cur_edge = edges_[cur_node->in_edges_[edge_i]];
      const int tail_end = cur_edge.tail_nodes_.size();
      if ((tail_end == tail_i) || (prune_edges && (*prune_edges)[cur_edge.id_])) {
        ++edge_i;
        tail_i = 0;
        continue;
      }
      const int tail_ni = cur_edge.tail_nodes_[tail_i];
      const int tail_color = color[tail_ni];
      if (tail_color == WHITE) {
        stack.push_back(DFSContext(cur_ni, edge_i, ++tail_i));
        cur_ni = tail_ni;
        cur_node = &nodes_[cur_ni];
        color[cur_ni] = GRAY;
        edge_i = 0;
        edge_end = cur_node->in_edges_.size();
        tail_i = 0;
      } else if (tail_color == BLACK) {
        ++tail_i;
      } else if (tail_color == GRAY) {
        // this can happen if, e.g., it is possible to rederive
        // a single cell in the CKY chart via a cycle.
        cerr << "Detected forbidden cycle in HG:\n";
        cerr << "  " << cur_edge.rule_->AsString() << endl;
        while(!stack.empty()) {
          const DFSContext& p = stack.back();
          cerr << "  " << edges_[nodes_[p.node].in_edges_[p.edge_iter]].rule_->AsString() << endl;
          stack.pop_back();
        }
        abort();
      }
    }
    color[cur_ni] = BLACK;
    reloc_node[cur_ni] = node_count++;
    if (prune_edges) {
      for (int i = 0; i < edge_end; ++i) {
        int ei = cur_node->in_edges_[i];
        if (!(*prune_edges)[ei])
          reloc_edge[cur_node->in_edges_[i]] = edge_count++;
      }
    } else {
      for (int i = 0; i < edge_end; ++i)
        reloc_edge[cur_node->in_edges_[i]] = edge_count++;
    }
  }
#ifndef HG_EDGES_TOPO_SORTED
  int ec = 0;
  for (int i = 0; i < reloc_edge.size(); ++i) {
    int& cp = reloc_edge[i];
    if (cp >= 0) { cp = ec++; }
  }
#endif

#if 0
  cerr << "TOPO:";
  for (int i = 0; i < reloc_node.size(); ++i)
    cerr << " " << reloc_node[i];
  cerr << endl;
  cerr << "EDGE:";
  for (int i = 0; i < reloc_edge.size(); ++i)
    cerr << " " << reloc_edge[i];
  cerr << endl;
#endif
  bool no_op = true;
  for (int i = 0; i < reloc_node.size() && no_op; ++i)
    if (reloc_node[i] != i) no_op = false;
  for (int i = 0; i < reloc_edge.size() && no_op; ++i)
    if (reloc_edge[i] != i) no_op = false;
  if (no_op) return;
  for (int i = 0; i < reloc_node.size(); ++i) {
    Node& node = nodes_[i];
    node.id_ = reloc_node[i];
    int c = 0;
    for (int j = 0; j < node.in_edges_.size(); ++j) {
      const int new_index = reloc_edge[node.in_edges_[j]];
      if (new_index >= 0)
        node.in_edges_[c++] = new_index;
    }
    node.in_edges_.resize(c);
    c = 0;
    for (int j = 0; j < node.out_edges_.size(); ++j) {
      const int new_index = reloc_edge[node.out_edges_[j]];
      if (new_index >= 0)
        node.out_edges_[c++] = new_index;
    }
    node.out_edges_.resize(c);
  }
  for (int i = 0; i < reloc_edge.size(); ++i) {
    Edge& edge = edges_[i];
    edge.id_ = reloc_edge[i];
    edge.head_node_ = reloc_node[edge.head_node_];
    for (int j = 0; j < edge.tail_nodes_.size(); ++j)
      edge.tail_nodes_[j] = reloc_node[edge.tail_nodes_[j]];
  }
  edges_.erase(remove_if(edges_.begin(), edges_.end(), BadId<Edge>()), edges_.end());
  nodes_.erase(remove_if(nodes_.begin(), nodes_.end(), BadId<Node>()), nodes_.end());
  sort(nodes_.begin(), nodes_.end(), IdCompare<Node>());
#ifndef HG_EDGES_TOPO_SORTED
  sort(edges_.begin(), edges_.end(), IdCompare<Edge>());
#endif
}

TRulePtr Hypergraph::kEPSRule;
TRulePtr Hypergraph::kUnaryRule;

void Hypergraph::EpsilonRemove(WordID eps) {
  if (!kEPSRule) {
    kEPSRule.reset(new TRule("[X] ||| <eps> ||| <eps>"));
    kUnaryRule.reset(new TRule("[X] ||| [X,1] ||| [X,1]"));
  }
  vector<bool> kill(edges_.size(), false);
  for (int i = 0; i < edges_.size(); ++i) {
    const Edge& edge = edges_[i];
    if (edge.tail_nodes_.empty() &&
        edge.rule_->f_.size() == 1 &&
        edge.rule_->f_[0] == eps) {
      kill[i] = true;
      if (!edge.feature_values_.empty()) {
        Node& node = nodes_[edge.head_node_];
        if (node.in_edges_.size() != 1) {
          cerr << "[WARNING] <eps> edge with features going into non-empty node - can't promote\n";
          // this *probably* means that there are multiple derivations of the
          // same sequence via different paths through the input forest
          // this needs to be investigated and fixed
        } else {
          for (int j = 0; j < node.out_edges_.size(); ++j)
            edges_[node.out_edges_[j]].feature_values_ += edge.feature_values_;
          // cerr << "PROMOTED " << edge.feature_values_ << endl;
	}
      }
    }
  }
  bool created_eps = false;
  PruneEdges(kill);
  for (int i = 0; i < nodes_.size(); ++i) {
    const Node& node = nodes_[i];
    if (node.in_edges_.empty()) {
      for (int j = 0; j < node.out_edges_.size(); ++j) {
        Edge& edge = edges_[node.out_edges_[j]];
        if (edge.rule_->Arity() == 2) {
          assert(edge.rule_->f_.size() == 2);
          assert(edge.rule_->e_.size() == 2);
          edge.rule_ = kUnaryRule;
          int cur = node.id_;
          int t = -1;
          assert(edge.tail_nodes_.size() == 2);
          for (int i = 0; i < 2; ++i) if (edge.tail_nodes_[i] != cur) { t = edge.tail_nodes_[i]; }
          assert(t != -1);
          edge.tail_nodes_.resize(1);
          edge.tail_nodes_[0] = t;
        } else {
          edge.rule_ = kEPSRule;
          edge.rule_->f_[0] = eps;
          edge.rule_->e_[0] = eps;
          edge.tail_nodes_.clear();
          created_eps = true;
        }
      }
    }
  }
  vector<bool> k2(edges_.size(), false);
  PruneEdges(k2);
  if (created_eps) EpsilonRemove(eps);
}

struct EdgeWeightSorter {
  const Hypergraph& hg;
  EdgeWeightSorter(const Hypergraph& h) : hg(h) {}
  bool operator()(int a, int b) const {
    return hg.edges_[a].edge_prob_ > hg.edges_[b].edge_prob_;
  }
};

void Hypergraph::SortInEdgesByEdgeWeights() {
  for (int i = 0; i < nodes_.size(); ++i) {
    Node& node = nodes_[i];
    sort(node.in_edges_.begin(), node.in_edges_.end(), EdgeWeightSorter(*this));
  }
}

Hypergraph* Hypergraph::CreateViterbiHypergraph(const vector<bool>* edges) const {
  vector<const Edge*> vit_edges;
  if (edges) {
    assert(edges->size() == edges_.size());
    Viterbi<vector<const Edge*>, ViterbiPathTraversal, prob_t, EdgeSelectEdgeWeightFunction>(*this, &vit_edges, ViterbiPathTraversal(), EdgeSelectEdgeWeightFunction(*edges));
  } else {
    Viterbi<vector<const Edge*>, ViterbiPathTraversal, prob_t, EdgeProb>(*this, &vit_edges);
  }
  map<int, int> old2new_node;
  int num_new_nodes = 0;
  for (int i = 0; i < vit_edges.size(); ++i) {
    const Edge& edge = *vit_edges[i];
    for (int j = 0; j < edge.tail_nodes_.size(); ++j)
      assert(old2new_node.count(edge.tail_nodes_[j]) > 0);
    if (old2new_node.count(edge.head_node_) == 0) {
      old2new_node[edge.head_node_] = num_new_nodes;
      ++num_new_nodes;
    }
  }
  Hypergraph* out = new Hypergraph(num_new_nodes, vit_edges.size(), is_linear_chain_);
  for (map<int, int>::iterator it = old2new_node.begin();
       it != old2new_node.end(); ++it) {
    const Node& old_node = nodes_[it->first];
    Node& new_node = out->nodes_[it->second];
    new_node.cat_ = old_node.cat_;
    new_node.id_ = it->second;
  }

  for (int i = 0; i < vit_edges.size(); ++i) {
    const Edge& old_edge = *vit_edges[i];
    Edge& new_edge = out->edges_[i];
    new_edge = old_edge;
    new_edge.id_ = i;
    const int new_head_node = old2new_node[old_edge.head_node_];
    new_edge.head_node_ = new_head_node;
    out->nodes_[new_head_node].in_edges_.push_back(i);
    for (int j = 0; j < old_edge.tail_nodes_.size(); ++j) {
      const int new_tail_node = old2new_node[old_edge.tail_nodes_[j]];
      new_edge.tail_nodes_[j] = new_tail_node;
      out->nodes_[new_tail_node].out_edges_.push_back(i);
    }
  }
  return out;
}

