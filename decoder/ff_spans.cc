#include "ff_spans.h"

#include <sstream>
#include <cassert>

#include "filelib.h"
#include "stringlib.h"
#include "sentence_metadata.h"
#include "lattice.h"
#include "fdict.h"
#include "verbose.h"

using namespace std;

SpanFeatures::SpanFeatures(const string& param) :
    kS(TD::Convert("S") * -1),
    kX(TD::Convert("X") * -1),
    use_collapsed_features_(false) {
  string mapfile = param;
  string valfile;
  vector<string> toks;
  Tokenize(param, ' ', &toks);
  if (toks.size() == 2) { mapfile = param[0]; valfile = param[1]; }
  if (mapfile.size() > 0) {
    int lc = 0;
    if (!SILENT) { cerr << "Reading word map for SpanFeatures from " << param << endl; }
    ReadFile rf(mapfile);
    istream& in = *rf.stream();
    string line;
    vector<WordID> v;
    while(in) {
      ++lc;
      getline(in, line);
      if (line.empty()) continue;
      v.clear();
      TD::ConvertSentence(line, &v);
      if (v.size() != 2) {
        cerr << "Error reading line " << lc << ": " << line << endl;
        abort();
      }
      word2class_[v[0]] = v[1];
    }
    word2class_[TD::Convert("<s>")] = TD::Convert("BOS");
    word2class_[TD::Convert("</s>")] = TD::Convert("EOS");
    oov_ = TD::Convert("OOV");
  }

  if (valfile.size() > 0) {
    use_collapsed_features_ = true;
    fid_beg_ = FD::Convert("SpanBegin");
    fid_end_ = FD::Convert("SpanEnd");
    fid_span_s_ = FD::Convert("SSpanContext");
    fid_span_ = FD::Convert("XSpanContext");
    ReadFile rf(valfile);
    if (!SILENT) { cerr << "  Loading span scores from " << valfile << endl; }
    istream& in = *rf.stream();
    string line;
    while(in) {
      getline(in, line);
      if (line.size() == 0 || line[0] == '#') { continue; }
      istringstream in(line);
      string feat_name;
      double weight;
      in >> feat_name >> weight;
      feat2val_[feat_name] = weight;
    }
  }
}

void SpanFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                         const Hypergraph::Edge& edge,
                                         const vector<const void*>& ant_contexts,
                                         SparseVector<double>* features,
                                         SparseVector<double>* estimated_features,
                                         void* context) const {
  assert(edge.j_ < end_span_ids_.size());
  assert(edge.j_ >= 0);
  assert(edge.i_ < beg_span_ids_.size());
  assert(edge.i_ >= 0);
  if (use_collapsed_features_) {
    features->set_value(fid_end_, end_span_vals_[edge.j_]);
    features->set_value(fid_beg_, beg_span_vals_[edge.i_]);
    if (edge.rule_->lhs_ == kS)
      features->set_value(fid_span_s_, span_vals_(edge.i_,edge.j_).second);
    else
      features->set_value(fid_span_, span_vals_(edge.i_,edge.j_).first);
  } else {  // non-collapsed features:
    features->set_value(end_span_ids_[edge.j_], 1);
    features->set_value(beg_span_ids_[edge.i_], 1);
    if (edge.rule_->lhs_ == kS)
      features->set_value(span_feats_(edge.i_,edge.j_).second, 1);
    else
      features->set_value(span_feats_(edge.i_,edge.j_).first, 1);
  }
}

WordID SpanFeatures::MapIfNecessary(const WordID& w) const {
  if (word2class_.empty()) return w;
  map<WordID,WordID>::const_iterator it = word2class_.find(w);
  if (it == word2class_.end()) return oov_;
  return it->second;
}

void SpanFeatures::PrepareForInput(const SentenceMetadata& smeta) {
  const Lattice& lattice = smeta.GetSourceLattice();
  const WordID eos = TD::Convert("</s>");
  const WordID bos = TD::Convert("<s>");
  beg_span_ids_.resize(lattice.size() + 1);
  end_span_ids_.resize(lattice.size() + 1);
  span_feats_.resize(lattice.size() + 1, lattice.size() + 1);
  if (use_collapsed_features_) {
    beg_span_vals_.resize(lattice.size() + 1);
    end_span_vals_.resize(lattice.size() + 1);
    span_vals_.resize(lattice.size() + 1, lattice.size() + 1);
  }
  for (int i = 0; i <= lattice.size(); ++i) {
    WordID word = eos;
    WordID bword = bos;
    if (i > 0)
      bword = lattice[i-1][0].label;
    bword = MapIfNecessary(bword);
    if (i < lattice.size())
      word = lattice[i][0].label;  // rather arbitrary for lattices
    word = MapIfNecessary(word);
    ostringstream sfid;
    sfid << "ES:" << TD::Convert(word);
    end_span_ids_[i] = FD::Convert(sfid.str());
    ostringstream bfid;
    bfid << "BS:" << TD::Convert(bword);
    beg_span_ids_[i] = FD::Convert(bfid.str());
    if (use_collapsed_features_) {
      end_span_vals_[i] = feat2val_[sfid.str()];
      beg_span_vals_[i] = feat2val_[bfid.str()];
    }
  }
  for (int i = 0; i <= lattice.size(); ++i) {
    WordID bword = bos;
    if (i > 0)
      bword = lattice[i-1][0].label;
    bword = MapIfNecessary(bword);
    for (int j = 0; j <= lattice.size(); ++j) {
      WordID word = eos;
      if (j < lattice.size())
        word = lattice[j][0].label;
      word = MapIfNecessary(word);
      ostringstream pf;
      pf << "SS:" << TD::Convert(bword) << "_" << TD::Convert(word);
      span_feats_(i,j).first = FD::Convert(pf.str());
      span_feats_(i,j).second = FD::Convert("S_" + pf.str());
      if (use_collapsed_features_) {
        span_vals_(i,j).first = feat2val_[pf.str()];
        span_vals_(i,j).second = feat2val_["S_" + pf.str()];
      }
    }
  } 
}

