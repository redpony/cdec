#include "hg_union.h"

#ifndef HAVE_OLD_CPP
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std { using std::tr1::unordered_set; }
#endif

#include "verbose.h"
#include "hg.h"
#include "sparse_vector.h"

using namespace std;

namespace HG {

static bool EdgesMatch(const HG::Edge& a, const Hypergraph& ahg, const HG::Edge& b, const Hypergraph& bhg) {
  const unsigned arity = a.tail_nodes_.size();
  if (arity != b.tail_nodes_.size()) return false;
  if (a.rule_->e() != b.rule_->e()) return false;
  if (a.rule_->f() != b.rule_->f()) return false;

  for (unsigned i = 0; i < arity; ++i)
    if (ahg.nodes_[a.tail_nodes_[i]].node_hash != bhg.nodes_[b.tail_nodes_[i]].node_hash) return false;
  const SparseVector<double> diff = a.feature_values_ - b.feature_values_;
  for (auto& kv : diff)
    if (fabs(kv.second) > 1e-6) return false;
  return true;
}

void Union(const Hypergraph& in, Hypergraph* out) {
  if (&in == out) return;
  if (out->nodes_.empty()) {
    out->nodes_ = in.nodes_;
    out->edges_ = in.edges_; return;
  }
  if (!in.AreNodesUniquelyIdentified()) {
    cerr << "Union: Nodes are not uniquely identified in input!\n";
    abort();
  }
  if (!out->AreNodesUniquelyIdentified()) {
    cerr << "Union: Nodes are not uniquely identified in output!\n";
    abort();
  }
  if (out->nodes_.back().node_hash != in.nodes_.back().node_hash) {
    cerr << "Union: Goal nodes are mismatched!\n  a=" << in.nodes_.back().node_hash << " b=" << out->nodes_.back().node_hash << "\n";
    abort();
  }
  const int cgoal = out->nodes_.back().id_;

  unordered_map<size_t, unsigned> h2n;
  for (const auto& node : out->nodes_)
    h2n[node.node_hash] = node.id_;
  for (const auto& node : in.nodes_) {
    if (h2n.count(node.node_hash) == 0) {
      HG::Node* new_node = out->AddNode(node.cat_);
      new_node->node_hash = node.node_hash;
      h2n[node.node_hash] = new_node->id_;
    }
  }

  double n_exists = 0;
  double n_created = 0;
  for (const auto& in_node : in.nodes_) {
    HG::Node& out_node = out->nodes_[h2n[in_node.node_hash]];
    //for (const auto oeid : out_node.in_edges_) {
      // TODO hash currently existing edges for quick check for duplication
    //}
    for (const auto ieid : in_node.in_edges_) {
      const HG::Edge& in_edge = in.edges_[ieid];
      // TODO: replace slow N^2 check with hashing
      bool edge_exists = false;
      for (const auto oeid : out_node.in_edges_) {
        if (EdgesMatch(in_edge, in, out->edges_[oeid], *out)) {
          edge_exists = true;
          break;
        }
      }
      if (!edge_exists) {
        const unsigned arity = in_edge.tail_nodes_.size();
        TailNodeVector t(arity);
        HG::Node& head = out->nodes_[h2n[in_node.node_hash]];
        for (unsigned i = 0; i < arity; ++i)
          t[i] = h2n[in.nodes_[in_edge.tail_nodes_[i]].node_hash];
        HG::Edge* new_edge = out->AddEdge(in_edge, t);
        out->ConnectEdgeToHeadNode(new_edge, &head);
        ++n_created;
        //cerr << "Created: " << new_edge->rule_->AsString() << " [head=" << new_edge->head_node_ << "]\n";
      } else {
        ++n_exists;
      }
      //  cerr << "Not created: " << in.edges_[ieid].rule_->AsString() << "\n";
      //}
    }
  }
  if (!SILENT)
    cerr << "  Union: edges_created=" << n_created
         << " edges_already_existing="
         << n_exists << "  ratio_new=" << (n_created / (n_exists + n_created))
         << endl;
  out->TopologicallySortNodesAndEdges(cgoal);
}

}

