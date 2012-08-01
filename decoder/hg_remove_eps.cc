#include "hg_remove_eps.h"

#include <cassert>

#include "trule.h"
#include "hg.h"

using namespace std;

namespace {
  TRulePtr kEPSRule;
  TRulePtr kUnaryRule;

  TRulePtr CreateUnaryRule(int lhs, int rhs) {
    if (!kUnaryRule) kUnaryRule.reset(new TRule("[X] ||| [X,1] ||| [X,1]"));
    TRule* r = new TRule(*kUnaryRule);
    assert(lhs < 0);
    assert(rhs < 0);
    r->lhs_ = lhs;
    r->f_[0] = rhs;
    return TRulePtr(r);
  }

  TRulePtr CreateEpsilonRule(int lhs, WordID eps) {
    if (!kEPSRule) kEPSRule.reset(new TRule("[X] ||| <eps> ||| <eps>"));
    TRule* r = new TRule(*kEPSRule);
    r->lhs_ = lhs;
    assert(lhs < 0);
    assert(eps > 0);
    r->e_[0] = eps;
    r->f_[0] = eps;
    return TRulePtr(r);
  }
}

void RemoveEpsilons(Hypergraph* g, WordID eps) {
  vector<bool> kill(g->edges_.size(), false);
  for (unsigned i = 0; i < g->edges_.size(); ++i) {
    const Hypergraph::Edge& edge = g->edges_[i];
    if (edge.tail_nodes_.empty() &&
        edge.rule_->f_.size() == 1 &&
        edge.rule_->f_[0] == eps) {
      kill[i] = true;
      if (!edge.feature_values_.empty()) {
        Hypergraph::Node& node = g->nodes_[edge.head_node_];
        if (node.in_edges_.size() != 1) {
          cerr << "[WARNING] <eps> edge with features going into non-empty node - can't promote\n";
          // this *probably* means that there are multiple derivations of the
          // same sequence via different paths through the input forest
          // this needs to be investigated and fixed
        } else {
          for (unsigned j = 0; j < node.out_edges_.size(); ++j)
            g->edges_[node.out_edges_[j]].feature_values_ += edge.feature_values_;
          // cerr << "PROMOTED " << edge.feature_values_ << endl;
	}
      }
    }
  }
  bool created_eps = false;
  g->PruneEdges(kill);
  for (unsigned i = 0; i < g->nodes_.size(); ++i) {
    const Hypergraph::Node& node = g->nodes_[i];
    if (node.in_edges_.empty()) {
      for (unsigned j = 0; j < node.out_edges_.size(); ++j) {
        Hypergraph::Edge& edge = g->edges_[node.out_edges_[j]];
        const int lhs = edge.rule_->lhs_;
        if (edge.rule_->Arity() == 2) {
          assert(edge.rule_->f_.size() == 2);
          assert(edge.rule_->e_.size() == 2);
          unsigned cur = node.id_;
          int t = -1;
          assert(edge.tail_nodes_.size() == 2);
          int rhs = 0;
          for (unsigned i = 0; i < 2u; ++i) if (edge.tail_nodes_[i] != cur) { t = edge.tail_nodes_[i]; rhs = edge.rule_->f_[i]; }
          assert(t != -1);
          edge.tail_nodes_.resize(1);
          edge.tail_nodes_[0] = t;
          edge.rule_ = CreateUnaryRule(lhs, rhs);
        } else {
          edge.rule_ = CreateEpsilonRule(lhs, eps);
          edge.tail_nodes_.clear();
          created_eps = true;
        }
      }
    }
  }
  vector<bool> k2(g->edges_.size(), false);
  g->PruneEdges(k2);
  if (created_eps) RemoveEpsilons(g, eps);
}

