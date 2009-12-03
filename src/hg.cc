#include "hg.h"

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

prob_t Hypergraph::ComputeEdgePosteriors(double scale, vector<prob_t>* posts) const {
  const ScaledEdgeProb weight(scale);
  SparseVector<double> pv;
  const double inside = InsideOutside<prob_t,
                  ScaledEdgeProb,
                  SparseVector<double>,
                  EdgeFeaturesWeightFunction>(*this, &pv, weight);
  posts->resize(edges_.size());
  for (int i = 0; i < edges_.size(); ++i)
    (*posts)[i] = prob_t(pv.value(i));
  return prob_t(inside);
}

prob_t Hypergraph::ComputeBestPathThroughEdges(vector<prob_t>* post) const {
  vector<prob_t> in(edges_.size());
  vector<prob_t> out(edges_.size());
  post->resize(edges_.size());

  vector<prob_t> ins_node_best(nodes_.size());
  for (int i = 0; i < nodes_.size(); ++i) {
    const Node& node = nodes_[i];
    prob_t& node_ins_best = ins_node_best[i];
    if (node.in_edges_.empty()) node_ins_best = prob_t::One();
    for (int j = 0; j < node.in_edges_.size(); ++j) {
      const Edge& edge = edges_[node.in_edges_[j]];
      prob_t& in_edge_sco = in[node.in_edges_[j]];
      in_edge_sco = edge.edge_prob_;
      for (int k = 0; k < edge.tail_nodes_.size(); ++k)
        in_edge_sco *= ins_node_best[edge.tail_nodes_[k]];
      if (in_edge_sco > node_ins_best) node_ins_best = in_edge_sco;
    }
  }
  const prob_t ins_sco = ins_node_best[nodes_.size() - 1];

  // sanity check
  int tots = 0;
  for (int i = 0; i < nodes_.size(); ++i) { if (nodes_[i].out_edges_.empty()) tots++; }
  assert(tots == 1);

  // compute outside scores, potentially using inside scores
  vector<prob_t> out_node_best(nodes_.size());
  for (int i = nodes_.size() - 1; i >= 0; --i) {
    const Node& node = nodes_[i];
    prob_t& node_out_best = out_node_best[node.id_];
    if (node.out_edges_.empty()) node_out_best = prob_t::One();
    for (int j = 0; j < node.out_edges_.size(); ++j) {
      const Edge& edge = edges_[node.out_edges_[j]];
      prob_t sco = edge.edge_prob_ * out_node_best[edge.head_node_];
      for (int k = 0; k < edge.tail_nodes_.size(); ++k) {
        if (edge.tail_nodes_[k] != i)
          sco *= ins_node_best[edge.tail_nodes_[k]];
      }
      if (sco > node_out_best) node_out_best = sco;
    }
    for (int j = 0; j < node.in_edges_.size(); ++j) {
      out[node.in_edges_[j]] = node_out_best;
    }
  }

  for (int i = 0; i < in.size(); ++i)
    (*post)[i] = in[i] * out[i];

  return ins_sco;
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

void Hypergraph::PruneEdges(const std::vector<bool>& prune_edge) {
  assert(prune_edge.size() == edges_.size());
  TopologicallySortNodesAndEdges(nodes_.size() - 1, &prune_edge);
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
  vector<bool> prune(edges_.size());
  //cerr << preserve_mask.size() << " " << edges_.size() << endl;
  int pc = 0;
  for (int i = 0; i < io.size(); ++i) {
    const bool prune_edge = (io[i] < cutoff);
    if (prune_edge) ++pc;
    prune[i] = (io[i] < cutoff);
    if (preserve_mask && (*preserve_mask)[i]) prune[i] = false;
  }
  cerr << "Beam pruning " << pc << "/" << io.size() << " edges\n";
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
    for (int i = 0; i < edge.tail_nodes_.size(); ++i) {
      cerr << "     " << edge.tail_nodes_[i] << " -> A_" << ei << ";\n";
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

int Hypergraph::MarkReachable(const Node& node,
                              vector<bool>* rmap,
                              const vector<bool>* prune_edges) const {
  int total = 0;
  if (!(*rmap)[node.id_]) {
    total = 1;
    (*rmap)[node.id_] = true;
    for (int i = 0; i < node.in_edges_.size(); ++i) {
      if (!(prune_edges && (*prune_edges)[node.in_edges_[i]])) {
        for (int j = 0; j < edges_[node.in_edges_[i]].tail_nodes_.size(); ++j)
         total += MarkReachable(nodes_[edges_[node.in_edges_[i]].tail_nodes_[j]], rmap, prune_edges);
      }
    }
  }
  return total;
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

void Hypergraph::TopologicallySortNodesAndEdges(int goal_index,
                                                const vector<bool>* prune_edges) {
  vector<Edge> sedges(edges_.size());
  // figure out which nodes are reachable from the goal
  vector<bool> reachable(nodes_.size(), false);
  int num_reachable = MarkReachable(nodes_[goal_index], &reachable, prune_edges);
  vector<Node> snodes(num_reachable); snodes.clear();

  // enumerate all reachable nodes in topologically sorted order
  vector<int> old_node_to_new_id(nodes_.size(), -1);
  vector<int> node_to_incount(nodes_.size(), -1);
  vector<bool> node_processed(nodes_.size(), false);
  typedef map<int, set<int> > PQueue;
  PQueue pri_q;
  for (int i = 0; i < nodes_.size(); ++i) {
    if (!reachable[i])
      continue;
    const int inedges = nodes_[i].in_edges_.size();
    int incount = inedges;
    for (int j = 0; j < inedges; ++j)
      if (edges_[nodes_[i].in_edges_[j]].tail_nodes_.size() == 0 ||
          (prune_edges && (*prune_edges)[nodes_[i].in_edges_[j]]))
        --incount;
    // cerr << &nodes_[i] <<" : incount=" << incount << "\tout=" << nodes_[i].out_edges_.size() << "\t(in-edges=" << inedges << ")\n";
    assert(node_to_incount[i] == -1);
    node_to_incount[i] = incount;
    pri_q[incount].insert(i);
  }

  int edge_count = 0;
  while (!pri_q.empty()) {
    PQueue::iterator iter = pri_q.find(0);
    assert(iter != pri_q.end());
    assert(!iter->second.empty());

    // get first node with incount = 0
    const int cur_index = *iter->second.begin();
    const Node& node = nodes_[cur_index];
    assert(reachable[cur_index]);
    //cerr << "node: " << node << endl;
    const int new_node_index = snodes.size();
    old_node_to_new_id[cur_index] = new_node_index;
    snodes.push_back(node);
    Node& new_node = snodes.back();
    new_node.id_ = new_node_index;
    new_node.out_edges_.clear();

    // fix up edges - we can now process the in edges and
    // the out edges of their tails
    int oi = 0;
    for (int i = 0; i < node.in_edges_.size(); ++i, ++oi) {
      if (prune_edges && (*prune_edges)[node.in_edges_[i]]) {
        --oi;
        continue;
      }
      new_node.in_edges_[oi] = edge_count;
      Edge& edge = sedges[edge_count];
      edge.id_ = edge_count;
      ++edge_count;
      const Edge& old_edge = edges_[node.in_edges_[i]];
      edge.rule_ = old_edge.rule_;
      edge.feature_values_ = old_edge.feature_values_;
      edge.head_node_ = new_node_index;
      edge.tail_nodes_.resize(old_edge.tail_nodes_.size());
      edge.edge_prob_ = old_edge.edge_prob_;
      edge.i_ = old_edge.i_;
      edge.j_ = old_edge.j_;
      edge.prev_i_ = old_edge.prev_i_;
      edge.prev_j_ = old_edge.prev_j_;
      for (int j = 0; j < old_edge.tail_nodes_.size(); ++j) {
        const Node& old_tail_node = nodes_[old_edge.tail_nodes_[j]];
        edge.tail_nodes_[j] = old_node_to_new_id[old_tail_node.id_];
        snodes[edge.tail_nodes_[j]].out_edges_.push_back(edge_count-1);
        assert(edge.tail_nodes_[j] != new_node_index);
      }
    }
    assert(oi <= new_node.in_edges_.size());
    new_node.in_edges_.resize(oi);

    for (int i = 0; i < node.out_edges_.size(); ++i) {
      const Edge& edge = edges_[node.out_edges_[i]];
      const int next_index = edge.head_node_;
      assert(cur_index != next_index);
      if (!reachable[next_index]) continue;
      if (prune_edges && (*prune_edges)[edge.id_]) continue;

      bool dontReduce = false;
      for (int j = 0; j < edge.tail_nodes_.size() && !dontReduce; ++j) {
        int tail_index = edge.tail_nodes_[j];
        dontReduce = (tail_index != cur_index) && !node_processed[tail_index];
      }
      if (dontReduce)
        continue;

      const int incount = node_to_incount[next_index];
      if (incount <= 0) {
        cerr << "incount = " << incount << ", should be > 0!\n";
        cerr << "do you have a cycle in your hypergraph?\n";
        abort();
      }
      PQueue::iterator it = pri_q.find(incount);
      assert(it != pri_q.end());
      it->second.erase(next_index);
      if (it->second.empty()) pri_q.erase(it);

      // reinsert node with reduced incount
      pri_q[incount-1].insert(next_index);
      --node_to_incount[next_index];
    }

    // remove node from set
    iter->second.erase(cur_index);
    if (iter->second.empty())
      pri_q.erase(iter);
    node_processed[cur_index] = true;
  }

  sedges.resize(edge_count);
  nodes_.swap(snodes);
  edges_.swap(sedges);
  assert(nodes_.back().out_edges_.size() == 0);
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

