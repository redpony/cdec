#include "hg_union.h"

#include "hg.h"

using namespace std;

namespace HG {

void Union(const Hypergraph& in, Hypergraph* out) {
  if (&in == out) return;
  if (out->nodes_.empty()) {
    out->nodes_ = in.nodes_;
    out->edges_ = in.edges_; return;
  }
  unsigned noff = out->nodes_.size();
  unsigned eoff = out->edges_.size();
  int ogoal = in.nodes_.size() - 1;
  int cgoal = noff - 1;
  // keep a single goal node, so add nodes.size - 1
  out->nodes_.resize(out->nodes_.size() + ogoal);
  // add all edges
  out->edges_.resize(out->edges_.size() + in.edges_.size());

  for (int i = 0; i < ogoal; ++i) {
    const Hypergraph::Node& on = in.nodes_[i];
    Hypergraph::Node& cn = out->nodes_[i + noff];
    cn.id_ = i + noff;
    cn.in_edges_.resize(on.in_edges_.size());
    for (unsigned j = 0; j < on.in_edges_.size(); ++j)
      cn.in_edges_[j] = on.in_edges_[j] + eoff;

    cn.out_edges_.resize(on.out_edges_.size());
    for (unsigned j = 0; j < on.out_edges_.size(); ++j)
      cn.out_edges_[j] = on.out_edges_[j] + eoff;
  }

  for (unsigned i = 0; i < in.edges_.size(); ++i) {
    const Hypergraph::Edge& oe = in.edges_[i];
    Hypergraph::Edge& ce = out->edges_[i + eoff];
    ce.id_ = i + eoff;
    ce.rule_ = oe.rule_;
    ce.feature_values_ = oe.feature_values_;
    if (oe.head_node_ == ogoal) {
      ce.head_node_ = cgoal;
      out->nodes_[cgoal].in_edges_.push_back(ce.id_);
    } else {
      ce.head_node_ = oe.head_node_ + noff;
    }
    ce.tail_nodes_.resize(oe.tail_nodes_.size());
    for (unsigned j = 0; j < oe.tail_nodes_.size(); ++j)
      ce.tail_nodes_[j] = oe.tail_nodes_[j] + noff;
  }

  out->TopologicallySortNodesAndEdges(cgoal);
}

}

