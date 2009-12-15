#include "aligner.h"

#include "array2d.h"
#include "hg.h"
#include "inside_outside.h"
#include <set>

using namespace std;

struct EdgeCoverageInfo {
  set<int> src_indices;
  set<int> trg_indices;
};

static bool is_digit(char x) { return x >= '0' && x <= '9'; }

boost::shared_ptr<Array2D<bool> > AlignerTools::ReadPharaohAlignmentGrid(const string& al) {
  int max_x = 0;
  int max_y = 0;
  int i = 0;
  while (i < al.size()) {
    int x = 0;
    while(i < al.size() && is_digit(al[i])) {
      x *= 10;
      x += al[i] - '0';
      ++i;
    }
    if (x > max_x) max_x = x;
    assert(i < al.size());
    assert(al[i] == '-');
    ++i;
    int y = 0;
    while(i < al.size() && is_digit(al[i])) {
      y *= 10;
      y += al[i] - '0';
      ++i;
    }
    if (y > max_y) max_y = y;
    while(i < al.size() && al[i] == ' ') { ++i; }
  }

  boost::shared_ptr<Array2D<bool> > grid(new Array2D<bool>(max_x + 1, max_y + 1));
  i = 0;
  while (i < al.size()) {
    int x = 0;
    while(i < al.size() && is_digit(al[i])) {
      x *= 10;
      x += al[i] - '0';
      ++i;
    }
    assert(i < al.size());
    assert(al[i] == '-');
    ++i;
    int y = 0;
    while(i < al.size() && is_digit(al[i])) {
      y *= 10;
      y += al[i] - '0';
      ++i;
    }
    (*grid)(x, y) = true;
    while(i < al.size() && al[i] == ' ') { ++i; }
  }
  // cerr << *grid << endl;
  return grid;
}

void AlignerTools::SerializePharaohFormat(const Array2D<bool>& alignment, ostream* out) {
  bool need_space = false;
  for (int i = 0; i < alignment.width(); ++i)
    for (int j = 0; j < alignment.height(); ++j)
      if (alignment(i,j)) {
        if (need_space) (*out) << ' '; else need_space = true;
        (*out) << i << '-' << j;
      }
  (*out) << endl;
}

// compute the coverage vectors of each edge
// prereq: all derivations yield the same string pair
void ComputeCoverages(const Hypergraph& g,
                      vector<EdgeCoverageInfo>* pcovs) {
  for (int i = 0; i < g.edges_.size(); ++i) {
    const Hypergraph::Edge& edge = g.edges_[i];
    EdgeCoverageInfo& cov = (*pcovs)[i];
    // no words
    if (edge.rule_->EWords() == 0 || edge.rule_->FWords() == 0)
      continue;
    // aligned to NULL (crf ibm variant only)
    if (edge.prev_i_ == -1 || edge.i_ == -1)
      continue;
    assert(edge.j_ >= 0);
    assert(edge.prev_j_ >= 0);
    if (edge.Arity() == 0) {
      for (int k = edge.i_; k < edge.j_; ++k)
        cov.trg_indices.insert(k);
      for (int k = edge.prev_i_; k < edge.prev_j_; ++k)
        cov.src_indices.insert(k);
    } else {
      // note: this code, which handles mixed NT and terminal
      // rules assumes that nodes uniquely define a src and trg
      // span.
      int k = edge.prev_i_;
      int j = 0;
      const vector<WordID>& f = edge.rule_->e();  // rules are inverted
      while (k < edge.prev_j_) {
        if (f[j] > 0) {
          cov.src_indices.insert(k);
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
      int tc = 0;
      const vector<WordID>& e = edge.rule_->f();  // rules are inverted
      k = edge.i_;
      j = 0;
      // cerr << edge.rule_->AsString() << endl;
      // cerr << "i=" << k << "  j=" << edge.j_ << endl;
      while (k < edge.j_) {
        //cerr << "  k=" << k << endl;
        if (e[j] > 0) {
          cov.trg_indices.insert(k);
          // cerr << "trg: " << k << endl;
          ++k;
          ++j;
        } else {
          assert(tc < edge.tail_nodes_.size());
          const Hypergraph::Node& tailnode = g.nodes_[edge.tail_nodes_[tc]];
          assert(tailnode.in_edges_.size() > 0);
          // any edge will do:
          const Hypergraph::Edge& rep_edge = g.edges_[tailnode.in_edges_.front()];
          // cerr << "t skip " << (rep_edge.j_ - rep_edge.i_) << endl;  // src span
          k += (rep_edge.j_ - rep_edge.i_);  // src span
          ++j;
          ++tc;
        }
      }
      //abort();
    }
  }
}

void AlignerTools::WriteAlignment(const string& input,
                                  const Lattice& ref,
                                  const Hypergraph& g,
                                  bool map_instead_of_viterbi) {
  if (!map_instead_of_viterbi) {
    assert(!"not implemented!");
  }
  vector<prob_t> edge_posteriors(g.edges_.size());
  {
    SparseVector<prob_t> posts;
    InsideOutside<prob_t, EdgeProb, SparseVector<prob_t>, TransitionEventWeightFunction>(g, &posts);
    for (int i = 0; i < edge_posteriors.size(); ++i)
      edge_posteriors[i] = posts[i];
  }
  vector<EdgeCoverageInfo> edge2cov(g.edges_.size());
  ComputeCoverages(g, &edge2cov);

  Lattice src;
  // currently only dealing with src text, even if the
  // model supports lattice translation (which it probably does)
  LatticeTools::ConvertTextToLattice(input, &src);
  // TODO assert that src is a "real lattice"

  Array2D<prob_t> align(src.size(), ref.size(), prob_t::Zero());
  for (int c = 0; c < g.edges_.size(); ++c) {
    const prob_t& p = edge_posteriors[c];
    const EdgeCoverageInfo& eci = edge2cov[c];
    for (set<int>::const_iterator si = eci.src_indices.begin();
         si != eci.src_indices.end(); ++si) {
      for (set<int>::const_iterator ti = eci.trg_indices.begin();
           ti != eci.trg_indices.end(); ++ti) {
        align(*si, *ti) += p;
      }
    }
  }
  prob_t threshold(0.9);
  const bool use_soft_threshold = true; // TODO configure

  Array2D<bool> grid(src.size(), ref.size(), false);
  for (int j = 0; j < ref.size(); ++j) {
    if (use_soft_threshold) {
      threshold = prob_t::Zero();
      for (int i = 0; i < src.size(); ++i)
        if (align(i, j) > threshold) threshold = align(i, j);
      //threshold *= prob_t(0.99);
    }
    for (int i = 0; i < src.size(); ++i)
      grid(i, j) = align(i, j) >= threshold;
  }
  cerr << align << endl;
  cerr << grid << endl;
  SerializePharaohFormat(grid, &cout);
};

