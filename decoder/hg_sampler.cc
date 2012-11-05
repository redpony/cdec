#include "hg_sampler.h"

#include <queue>

#include "viterbi.h"
#include "inside_outside.h"

using namespace std;

struct SampledDerivationWeightFunction {
  typedef double Weight;
  explicit SampledDerivationWeightFunction(const vector<bool>& sampled) : sampled_edges(sampled) {}
  double operator()(const Hypergraph::Edge& e) const {
    return static_cast<double>(sampled_edges[e.id_]);
  }
  const vector<bool>& sampled_edges;
};

void HypergraphSampler::sample_hypotheses(const Hypergraph& hg,
                                          unsigned n,
                                          MT19937* rng,
                                          vector<Hypothesis>* hypos) {
  hypos->clear();
  hypos->resize(n);

  // compute inside probabilities
  vector<prob_t> node_probs;
  Inside<prob_t, EdgeProb>(hg, &node_probs, EdgeProb());

  vector<bool> sampled_edges(hg.edges_.size());
  queue<unsigned> q;
  SampleSet<prob_t> ss;
  for (unsigned i = 0; i < n; ++i) {
    fill(sampled_edges.begin(), sampled_edges.end(), false);
    // sample derivation top down
    assert(q.empty());
    Hypothesis& hyp = (*hypos)[i];
    SparseVector<double>& deriv_features = hyp.fmap;
    q.push(hg.nodes_.size() - 1);
    prob_t& model_score = hyp.model_score;
    model_score = prob_t::One();
    while(!q.empty()) {
      unsigned cur_node_id = q.front();
      q.pop();
      const Hypergraph::Node& node = hg.nodes_[cur_node_id];
      const unsigned num_in_edges = node.in_edges_.size();
      unsigned sampled_edge_idx = 0;
      if (num_in_edges == 1) {
        sampled_edge_idx = node.in_edges_[0];
      } else {
        assert(num_in_edges > 1);
        ss.clear();
        for (unsigned j = 0; j < num_in_edges; ++j) {
          const Hypergraph::Edge& edge = hg.edges_[node.in_edges_[j]];
          prob_t p = edge.edge_prob_;   // edge weight
          for (unsigned k = 0; k < edge.tail_nodes_.size(); ++k)
            p *= node_probs[edge.tail_nodes_[k]];  // tail node inside weight
          ss.add(p);
        }
        sampled_edge_idx = node.in_edges_[rng->SelectSample(ss)];
      }
      sampled_edges[sampled_edge_idx] = true;
      const Hypergraph::Edge& sampled_edge = hg.edges_[sampled_edge_idx];
      deriv_features += sampled_edge.feature_values_;
      model_score *= sampled_edge.edge_prob_;
      //sampled_deriv->push_back(sampled_edge_idx);
      for (unsigned j = 0; j < sampled_edge.tail_nodes_.size(); ++j) {
        q.push(sampled_edge.tail_nodes_[j]);
      }
    }
    Viterbi(hg, &hyp.words, ESentenceTraversal(), SampledDerivationWeightFunction(sampled_edges));
  }
}

void HypergraphSampler::sample_trees(const Hypergraph& hg,
                                     unsigned n,
                                     MT19937* rng,
                                     vector<string>* trees) {
  trees->clear();
  trees->resize(n);

  // compute inside probabilities
  vector<prob_t> node_probs;
  Inside<prob_t, EdgeProb>(hg, &node_probs, EdgeProb());

  vector<bool> sampled_edges(hg.edges_.size());
  queue<unsigned> q;
  SampleSet<prob_t> ss;
  for (unsigned i = 0; i < n; ++i) {
    fill(sampled_edges.begin(), sampled_edges.end(), false);
    // sample derivation top down
    assert(q.empty());
    q.push(hg.nodes_.size() - 1);
    prob_t model_score = prob_t::One();
    while(!q.empty()) {
      unsigned cur_node_id = q.front();
      q.pop();
      const Hypergraph::Node& node = hg.nodes_[cur_node_id];
      const unsigned num_in_edges = node.in_edges_.size();
      unsigned sampled_edge_idx = 0;
      if (num_in_edges == 1) {
        sampled_edge_idx = node.in_edges_[0];
      } else {
        assert(num_in_edges > 1);
        ss.clear();
        for (unsigned j = 0; j < num_in_edges; ++j) {
          const Hypergraph::Edge& edge = hg.edges_[node.in_edges_[j]];
          prob_t p = edge.edge_prob_;   // edge weight
          for (unsigned k = 0; k < edge.tail_nodes_.size(); ++k)
            p *= node_probs[edge.tail_nodes_[k]];  // tail node inside weight
          ss.add(p);
        }
        sampled_edge_idx = node.in_edges_[rng->SelectSample(ss)];
      }
      sampled_edges[sampled_edge_idx] = true;
      const Hypergraph::Edge& sampled_edge = hg.edges_[sampled_edge_idx];
      model_score *= sampled_edge.edge_prob_;
      //sampled_deriv->push_back(sampled_edge_idx);
      for (unsigned j = 0; j < sampled_edge.tail_nodes_.size(); ++j) {
        q.push(sampled_edge.tail_nodes_[j]);
      }
    }
    vector<WordID> tmp;
    Viterbi(hg, &tmp, ETreeTraversal(), SampledDerivationWeightFunction(sampled_edges));
    (*trees)[i] = TD::GetString(tmp);
  }
}

