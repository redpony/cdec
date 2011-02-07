#include "ff_spans.h"

#include <sstream>
#include <cassert>

#include "sentence_metadata.h"
#include "lattice.h"
#include "fdict.h"

using namespace std;

SpanFeatures::SpanFeatures(const string& param) :
  kS(TD::Convert("S") * -1),
  kX(TD::Convert("X") * -1) {}

void SpanFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                         const Hypergraph::Edge& edge,
                                         const vector<const void*>& ant_contexts,
                                         SparseVector<double>* features,
                                         SparseVector<double>* estimated_features,
                                         void* context) const {
//  char& res = *static_cast<char*>(context);
//  res = edge.j_ - edge.i_;
//  assert(res >= 0);
  assert(edge.j_ < end_span_ids_.size());
  assert(edge.j_ >= 0);
  features->set_value(end_span_ids_[edge.j_], 1);
  assert(edge.i_ < beg_span_ids_.size());
  assert(edge.i_ >= 0);
  features->set_value(beg_span_ids_[edge.i_], 1);
  features->set_value(span_feats_(edge.i_,edge.j_), 1);
  if (edge.Arity() == 2) {
    const TRule& rule = *edge.rule_;
    if (rule.f_[0] == kS && rule.f_[1] == kX) {
//      char x_width = *static_cast<const char*>(ant_contexts[1]);
    }
  }
}

void SpanFeatures::PrepareForInput(const SentenceMetadata& smeta) {
  const Lattice& lattice = smeta.GetSourceLattice();
  const WordID eos = TD::Convert("</s>");
  const WordID bos = TD::Convert("<s>");
  beg_span_ids_.resize(lattice.size() + 1);
  end_span_ids_.resize(lattice.size() + 1);
  for (int i = 0; i <= lattice.size(); ++i) {
    WordID word = eos;
    WordID bword = bos;
    if (i > 0)
      bword = lattice[i-1][0].label;
    if (i < lattice.size())
      word = lattice[i][0].label;  // rather arbitrary for lattices
    ostringstream sfid;
    sfid << "ES:" << TD::Convert(word);
    end_span_ids_[i] = FD::Convert(sfid.str());
    ostringstream bfid;
    bfid << "BS:" << TD::Convert(bword);
    beg_span_ids_[i] = FD::Convert(bfid.str());
  }
  span_feats_.resize(lattice.size() + 1, lattice.size() + 1);
  for (int i = 0; i <= lattice.size(); ++i) {
    WordID bword = bos;
    if (i > 0)
      bword = lattice[i-1][0].label;
    for (int j = 0; j <= lattice.size(); ++j) {
      WordID word = eos;
      if (j < lattice.size())
        word = lattice[j][0].label;
      ostringstream pf;
      pf << "SS:" << TD::Convert(bword) << "_" << TD::Convert(word);
      span_feats_(i,j) = FD::Convert(pf.str());
    }
  } 
}

