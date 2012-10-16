#include "ff_spans.h"

#include <sstream>
#include <cassert>
#include <cmath>

#include "hg.h"
#include "tdict.h"
#include "filelib.h"
#include "stringlib.h"
#include "sentence_metadata.h"
#include "lattice.h"
#include "fdict.h"
#include "verbose.h"

using namespace std;

namespace {
  string Escape(const string& x) {
    string y = x;
    for (int i = 0; i < y.size(); ++i) {
      if (y[i] == '=') y[i]='_';
      if (y[i] == ';') y[i]='_';
    }
    return y;
  }
}

// log transform to make long spans cluster together
// but preserve differences
int SpanSizeTransform(unsigned span_size) {
  if (!span_size) return 0;
  return static_cast<int>(log(span_size+1) / log(1.39)) - 1;
}

SpanFeatures::SpanFeatures(const string& param) :
    kS(TD::Convert("S") * -1),
    kX(TD::Convert("X") * -1),
    use_collapsed_features_(false) {
  string mapfile = param;
  string valfile;
  vector<string> toks;
  Tokenize(param, ' ', &toks);
  if (toks.size() == 2) { mapfile = toks[0]; valfile = toks[1]; }
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
    word2class_[TD::Convert("BOS")] = TD::Convert("BOS");
    word2class_[TD::Convert("EOS")] = TD::Convert("EOS");
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
    features->set_value(end_bigram_ids_[edge.j_], 1);
    features->set_value(beg_bigram_ids_[edge.i_], 1);
    if (edge.rule_->lhs_ == kS) {
      features->set_value(span_feats_(edge.i_,edge.j_).second, 1);
      features->set_value(len_span_feats_(edge.i_,edge.j_).second, 1);
    } else {
      features->set_value(span_feats_(edge.i_,edge.j_).first, 1);
      features->set_value(len_span_feats_(edge.i_,edge.j_).first, 1);
    }
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
  const WordID eos = TD::Convert("EOS");  // right of the last source word
  const WordID bos = TD::Convert("BOS");  // left of the first source word
  beg_span_ids_.resize(lattice.size() + 1);
  end_span_ids_.resize(lattice.size() + 1);
  span_feats_.resize(lattice.size() + 1, lattice.size() + 1);
  beg_bigram_ids_.resize(lattice.size() + 1);
  end_bigram_ids_.resize(lattice.size() + 1);
  len_span_feats_.resize(lattice.size() + 1, lattice.size() + 1);
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
    end_span_ids_[i] = FD::Convert(Escape(sfid.str()));
    ostringstream esbiid;
    esbiid << "EBI:" << TD::Convert(bword) << "_" << TD::Convert(word);
    end_bigram_ids_[i] = FD::Convert(Escape(esbiid.str()));
    ostringstream bsbiid;
    bsbiid << "BBI:" << TD::Convert(bword) << "_" << TD::Convert(word);
    beg_bigram_ids_[i] = FD::Convert(Escape(bsbiid.str()));
    ostringstream bfid;
    bfid << "BS:" << TD::Convert(bword);
    beg_span_ids_[i] = FD::Convert(Escape(bfid.str()));
    if (use_collapsed_features_) {
      end_span_vals_[i] = feat2val_[Escape(sfid.str())] + feat2val_[Escape(esbiid.str())];
      beg_span_vals_[i] = feat2val_[Escape(bfid.str())] + feat2val_[Escape(bsbiid.str())];
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
      pf << "S:" << TD::Convert(bword) << "_" << TD::Convert(word);
      span_feats_(i,j).first = FD::Convert(Escape(pf.str()));
      span_feats_(i,j).second = FD::Convert(Escape("S_" + pf.str()));
      ostringstream lf;
      const unsigned span_size = (i < j ? j - i : i - j);
      lf << "LS:" << SpanSizeTransform(span_size) << "_" << TD::Convert(bword) << "_" << TD::Convert(word);
      len_span_feats_(i,j).first = FD::Convert(Escape(lf.str()));
      len_span_feats_(i,j).second = FD::Convert(Escape("S_" + lf.str()));
      if (use_collapsed_features_) {
        span_vals_(i,j).first = feat2val_[Escape(pf.str())] + feat2val_[Escape(lf.str())];
        span_vals_(i,j).second = feat2val_[Escape("S_" + pf.str())] + feat2val_[Escape("S_" + lf.str())];
      }
    }
  } 
}

inline bool IsArity2RuleReordered(const TRule& rule) {
  const vector<WordID>& e = rule.e_;
  for (int i = 0; i < e.size(); ++i) {
    if (e[i] <= 0) { return e[i] < 0; }
  }
  cerr << "IsArity2RuleReordered failed on:\n" << rule.AsString() << endl;
  abort();
}

// Chiang, Marton, Resnik 2008 "fine-grained" reordering features
CMR2008ReorderingFeatures::CMR2008ReorderingFeatures(const std::string& param) :
    kS(TD::Convert("S") * -1),
    use_collapsed_features_(false) {
  if (param.size() > 0) {
    use_collapsed_features_ = true;
    assert(!"not implemented"); // TODO
  } else {
    unconditioned_fids_.first = FD::Convert("CMRMono");
    unconditioned_fids_.second = FD::Convert("CMRReorder");
    fids_.resize(16); fids_[0].first = fids_[0].second = -1;
    // since I use a log transform, I go a bit higher than David, who bins everything > 10
    for (int span_size = 1; span_size <= 15; ++span_size) {
      ostringstream m, r;
      m << "CMRMono_" << SpanSizeTransform(span_size);
      fids_[span_size].first = FD::Convert(m.str());
      r << "CMRReorder_" << SpanSizeTransform(span_size);
      fids_[span_size].second = FD::Convert(r.str());
    }
  }
}

void CMR2008ReorderingFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                         const Hypergraph::Edge& edge,
                                         const vector<const void*>& ant_contexts,
                                         SparseVector<double>* features,
                                         SparseVector<double>* estimated_features,
                                         void* context) const {
  if (edge.Arity() != 2) return;
  if (edge.rule_->lhs_ == kS) return;
  assert(edge.i_ >= 0);
  assert(edge.j_ > edge.i_);
  const bool is_reordered = IsArity2RuleReordered(*edge.rule_);
  const unsigned span_size = edge.j_ - edge.i_;
  if (use_collapsed_features_) {
    assert(!"not impl"); // TODO
  } else {
    if (is_reordered) {
      features->set_value(unconditioned_fids_.second, 1.0);
      features->set_value(fids_[span_size].second, 1.0);
    } else {
      features->set_value(unconditioned_fids_.first, 1.0);
      features->set_value(fids_[span_size].first, 1.0);
    }
  }
}

