#include "aligner.h"

#include <cstdio>
#include <set>

#include <boost/scoped_ptr.hpp>

#include "array2d.h"
#include "hg.h"
#include "kbest.h"
#include "sentence_metadata.h"
#include "inside_outside.h"
#include "viterbi.h"
#include "alignment_io.h"

using namespace std;

// used with lexical models since they may not fully generate the
// source string
void SourceEdgeCoveragesUsingParseIndices(const Hypergraph& g,
                                          vector<set<int> >* src_cov) {
  src_cov->clear();
  src_cov->resize(g.edges_.size());

  for (int i = 0; i < g.edges_.size(); ++i) {
    const Hypergraph::Edge& edge = g.edges_[i];
    set<int>& cov = (*src_cov)[i];
    // no words
    if (edge.rule_->EWords() == 0 || edge.rule_->FWords() == 0)
      continue;
    // aligned to NULL (crf ibm variant only)
    if (edge.prev_i_ == -1 || edge.i_ == -1) {
      cov.insert(-1);
      continue;
    }
    assert(edge.j_ >= 0);
    assert(edge.prev_j_ >= 0);
    if (edge.Arity() == 0) {
      for (int k = edge.prev_i_; k < edge.prev_j_; ++k)
        cov.insert(k);
    } else {
      // note: this code, which handles mixed NT and terminal
      // rules assumes that nodes uniquely define a src and trg
      // span.
      int k = edge.prev_i_;
      int j = 0;
      const vector<WordID>& f = edge.rule_->e();  // rules are inverted
      while (k < edge.prev_j_) {
        if (f[j] > 0) {
          cov.insert(k);
          // cerr << "src: " << k << endl;
          ++k;
          ++j;
        } else {
          const Hypergraph::Node& tailnode = g.nodes_[edge.tail_nodes_[-f[j]]];
          assert(tailnode.in_edges_.size() > 0);
          // any edge will do:
          const Hypergraph::Edge& rep_edge = g.edges_[tailnode.in_edges_.front()];
          //cerr << "skip " << (rep_edge.prev_j_ - rep_edge.prev_i_) << endl;  // src span
          k += (rep_edge.prev_j_ - rep_edge.prev_i_);  // src span
          ++j;
        }
      }
    }
  }
}

int SourceEdgeCoveragesUsingTree(const Hypergraph& g,
                                 int node_id,
                                 int span_start,
                                 vector<int>* spans,
                                 vector<set<int> >* src_cov) {
  const Hypergraph::Node& node = g.nodes_[node_id];
  int k = -1;
  for (int i = 0; i < node.in_edges_.size(); ++i) {
    const int edge_id = node.in_edges_[i];
    const Hypergraph::Edge& edge = g.edges_[edge_id];
    set<int>& cov = (*src_cov)[edge_id];
    const vector<WordID>& f = edge.rule_->e();  // rules are inverted
    int j = 0;
    k = span_start;
    while (j < f.size()) {
      if (f[j] > 0) {
        cov.insert(k);
        ++k;
        ++j;
      } else {
        const int tail_node_id = edge.tail_nodes_[-f[j]];
        int &right_edge = (*spans)[tail_node_id];
        if (right_edge < 0)
          right_edge = SourceEdgeCoveragesUsingTree(g, tail_node_id, k, spans, src_cov);
        k = right_edge;
        ++j;
      }
    }
  }
  return k;
}

void SourceEdgeCoveragesUsingTree(const Hypergraph& g,
                                  vector<set<int> >* src_cov) {
  src_cov->clear();
  src_cov->resize(g.edges_.size());
  vector<int> span_sizes(g.nodes_.size(), -1);
  SourceEdgeCoveragesUsingTree(g, g.nodes_.size() - 1, 0, &span_sizes, src_cov);
}

int TargetEdgeCoveragesUsingTree(const Hypergraph& g,
                                 int node_id,
                                 int span_start,
                                 vector<int>* spans,
                                 vector<set<int> >* trg_cov) {
  const Hypergraph::Node& node = g.nodes_[node_id];
  int k = -1;
  for (int i = 0; i < node.in_edges_.size(); ++i) {
    const int edge_id = node.in_edges_[i];
    const Hypergraph::Edge& edge = g.edges_[edge_id];
    set<int>& cov = (*trg_cov)[edge_id];
    int ntc = 0;
    const vector<WordID>& e = edge.rule_->f();  // rules are inverted
    int j = 0;
    k = span_start;
    while (j < e.size()) {
      if (e[j] > 0) {
        cov.insert(k);
        ++k;
        ++j;
      } else {
        const int tail_node_id = edge.tail_nodes_[ntc];
        ++ntc;
        int &right_edge = (*spans)[tail_node_id];
        if (right_edge < 0)
          right_edge = TargetEdgeCoveragesUsingTree(g, tail_node_id, k, spans, trg_cov);
        k = right_edge;
        ++j;
      }
    }
    // cerr << "node=" << node_id << ": k=" << k << endl;
  }
  return k;
}

void TargetEdgeCoveragesUsingTree(const Hypergraph& g,
                                  vector<set<int> >* trg_cov) {
  trg_cov->clear();
  trg_cov->resize(g.edges_.size());
  vector<int> span_sizes(g.nodes_.size(), -1);
  TargetEdgeCoveragesUsingTree(g, g.nodes_.size() - 1, 0, &span_sizes, trg_cov);
}

struct TransitionEventWeightFunction {
  typedef SparseVector<prob_t> Result;
  inline SparseVector<prob_t> operator()(const Hypergraph::Edge& e) const {
    SparseVector<prob_t> result;
    result.set_value(e.id_, e.edge_prob_);
    return result;
  }
};

inline void WriteProbGrid(const Array2D<prob_t>& m, ostream* pos) {
  ostream& os = *pos;
  char b[1024];
  for (int i=0; i<m.width(); ++i) {
    for (int j=0; j<m.height(); ++j) {
      if (m(i,j) == prob_t::Zero()) {
        os << "\t---X---";
      } else {
        snprintf(b, 1024, "%0.5f", m(i,j).as_float());
        os << '\t' << b;
      }
    }
    os << '\n';
  }
}

// this code is rather complicated since it must deal with generating alignments
// when lattices are specified as input as well as with models that do not generate
// full sentence pairs (like lexical alignment models)
void AlignerTools::WriteAlignment(const Lattice& src_lattice,
                                  const Lattice& trg_lattice,
                                  const Hypergraph& in_g,
                                  ostream* out,
                                  bool map_instead_of_viterbi,
                                  int k_best, // must = 0 if MAP
                                  const vector<bool>* edges) {
  bool fix_up_src_spans = false;
  if (k_best > 1 && edges) {
    cerr << "ERROR: cannot request multiple best alignments and provide an edge set!\n";
    abort();
  }
  if (map_instead_of_viterbi) {
    if (k_best != 0) {
      cerr << "WARNING: K-best alignment extraction not available for MAP, use --aligner_use_viterbi\n";
    }
    k_best = 1;
  } else {
    if (k_best == 0) k_best = 1;
  }
  const Hypergraph* g = &in_g;
  HypergraphP new_hg;
  if (!src_lattice.IsSentence() ||
      !trg_lattice.IsSentence()) {
    if (map_instead_of_viterbi) {
      cerr << "  Lattice alignment: using Viterbi instead of MAP alignment\n";
    }
    map_instead_of_viterbi = false;
    fix_up_src_spans = !src_lattice.IsSentence();
  }

  KBest::KBestDerivations<vector<Hypergraph::Edge const*>, ViterbiPathTraversal> kbest(in_g, k_best);
  boost::scoped_ptr<vector<bool> > kbest_edges;

  for (int best = 0; best < k_best; ++best) {
    const KBest::KBestDerivations<vector<Hypergraph::Edge const*>, ViterbiPathTraversal>::Derivation* d = NULL;
    if (!map_instead_of_viterbi) {
      d = kbest.LazyKthBest(in_g.nodes_.size() - 1, best);
      if (!d) break;  // there are fewer than k_best derivations!
      const vector<Hypergraph::Edge const*>& yield = d->yield;
      kbest_edges.reset(new vector<bool>(in_g.edges_.size(), false));
      for (int i = 0; i < yield.size(); ++i) {
        assert(yield[i]->id_ < kbest_edges->size());
        (*kbest_edges)[yield[i]->id_] = true;
      }
    }
    if (!map_instead_of_viterbi || edges) {
      if (kbest_edges) edges = kbest_edges.get();
      new_hg = in_g.CreateViterbiHypergraph(edges);
      for (int i = 0; i < new_hg->edges_.size(); ++i)
        new_hg->edges_[i].edge_prob_ = prob_t::One();
      g = new_hg.get();
    }

    vector<prob_t> edge_posteriors(g->edges_.size(), prob_t::Zero());
    vector<WordID> trg_sent;
    vector<WordID> src_sent;
    if (fix_up_src_spans) {
      ViterbiESentence(*g, &src_sent);
    } else {
      src_sent.resize(src_lattice.size());
      for (int i = 0; i < src_sent.size(); ++i)
        src_sent[i] = src_lattice[i][0].label;
    }

    ViterbiFSentence(*g, &trg_sent);

    if (edges || !map_instead_of_viterbi) {
      for (int i = 0; i < edge_posteriors.size(); ++i)
        edge_posteriors[i] = prob_t::One();
    } else {
      SparseVector<prob_t> posts;
      const prob_t z = InsideOutside<prob_t, EdgeProb, SparseVector<prob_t>, TransitionEventWeightFunction>(*g, &posts);
      for (int i = 0; i < edge_posteriors.size(); ++i)
        edge_posteriors[i] = posts.value(i) / z;
    }
    vector<set<int> > src_cov(g->edges_.size());
    vector<set<int> > trg_cov(g->edges_.size());
    TargetEdgeCoveragesUsingTree(*g, &trg_cov);

    if (fix_up_src_spans)
      SourceEdgeCoveragesUsingTree(*g, &src_cov);
    else
      SourceEdgeCoveragesUsingParseIndices(*g, &src_cov);

    // figure out the src and reference size;
    int src_size = src_sent.size();
    int ref_size = trg_sent.size();
    Array2D<prob_t> align(src_size + 1, ref_size, prob_t::Zero());
    for (int c = 0; c < g->edges_.size(); ++c) {
      const prob_t& p = edge_posteriors[c];
      const set<int>& srcs = src_cov[c];
      const set<int>& trgs = trg_cov[c];
      for (set<int>::const_iterator si = srcs.begin();
           si != srcs.end(); ++si) {
        for (set<int>::const_iterator ti = trgs.begin();
             ti != trgs.end(); ++ti) {
          align(*si + 1, *ti) += p;
        }
      }
    }
    new_hg.reset();
    //if (g != &in_g) { g.reset(); }

    prob_t threshold(0.9);
    const bool use_soft_threshold = true; // TODO configure

    Array2D<bool> grid(src_size, ref_size, false);
    for (int j = 0; j < ref_size; ++j) {
      if (use_soft_threshold) {
        threshold = prob_t::Zero();
        for (int i = 0; i <= src_size; ++i)
          if (align(i, j) > threshold) threshold = align(i, j);
        //threshold *= prob_t(0.99);
      }
      for (int i = 0; i < src_size; ++i)
        grid(i, j) = align(i+1, j) >= threshold;
    }
    if (out == &cout && k_best < 2) {
      // TODO need to do some sort of verbose flag
      WriteProbGrid(align, &cerr);
      cerr << grid << endl;
    }
    (*out) << TD::GetString(src_sent) << " ||| " << TD::GetString(trg_sent) << " ||| ";
    AlignmentIO::SerializePharaohFormat(grid, out);
  }
};

