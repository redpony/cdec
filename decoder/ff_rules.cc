#include "ff_rules.h"

#include <sstream>
#include <cassert>
#include <cmath>

#include "filelib.h"
#include "stringlib.h"
#include "sentence_metadata.h"
#include "lattice.h"
#include "fdict.h"
#include "verbose.h"
#include "tdict.h"
#include "hg.h"
#include "trule.h"

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

RuleIdentityFeatures::RuleIdentityFeatures(const std::string& param) {
}

void RuleIdentityFeatures::PrepareForInput(const SentenceMetadata& smeta) {
//  std::map<const TRule*, SparseVector<double> >
  rule2_fid_.clear();
}

void RuleIdentityFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                         const Hypergraph::Edge& edge,
                                         const vector<const void*>& ant_contexts,
                                         SparseVector<double>* features,
                                         SparseVector<double>* estimated_features,
                                         void* context) const {
  map<const TRule*, int>::iterator it = rule2_fid_.find(edge.rule_.get());
  if (it == rule2_fid_.end()) {
    const TRule& rule = *edge.rule_;
    ostringstream os;
    os << "R:";
    if (rule.lhs_ < 0) os << TD::Convert(-rule.lhs_) << ':';
    for (unsigned i = 0; i < rule.f_.size(); ++i) {
      if (i > 0) os << '_';
      WordID w = rule.f_[i];
      if (w < 0) { os << 'N'; w = -w; }
      assert(w > 0);
      os << TD::Convert(w);
    }
    os << ':';
    for (unsigned i = 0; i < rule.e_.size(); ++i) {
      if (i > 0) os << '_';
      WordID w = rule.e_[i];
      if (w <= 0) {
        os << 'N' << (1-w);
      } else {
        os << TD::Convert(w);
      }
    }
    it = rule2_fid_.insert(make_pair(&rule, FD::Convert(Escape(os.str())))).first;
  }
  features->add_value(it->second, 1);
}

RuleWordAlignmentFeatures::RuleWordAlignmentFeatures(const std::string& param) {
}

void RuleWordAlignmentFeatures::PrepareForInput(const SentenceMetadata& smeta) {
}

void RuleWordAlignmentFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                         const Hypergraph::Edge& edge,
                                         const vector<const void*>& ant_contexts,
                                         SparseVector<double>* features,
                                         SparseVector<double>* estimated_features,
                                         void* context) const {
  const TRule& rule = *edge.rule_;
  ostringstream os;
  vector<AlignmentPoint> als = rule.als(); 
  std::vector<AlignmentPoint>::const_iterator xx = als.begin();
  for (; xx != als.end(); ++xx) {
    os << "WA:" <<  TD::Convert(rule.f_[xx->s_]) << ":" << TD::Convert(rule.e_[xx->t_]);
  }
  features->add_value(FD::Convert(Escape(os.str())), 1);
}

RuleSourceBigramFeatures::RuleSourceBigramFeatures(const std::string& param) {
}

void RuleSourceBigramFeatures::PrepareForInput(const SentenceMetadata& smeta) {
//  std::map<const TRule*, SparseVector<double> >
  rule2_feats_.clear();
}

void RuleSourceBigramFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                         const Hypergraph::Edge& edge,
                                         const vector<const void*>& ant_contexts,
                                         SparseVector<double>* features,
                                         SparseVector<double>* estimated_features,
                                         void* context) const {
  map<const TRule*, SparseVector<double> >::iterator it = rule2_feats_.find(edge.rule_.get());
  if (it == rule2_feats_.end()) {
    const TRule& rule = *edge.rule_;
    it = rule2_feats_.insert(make_pair(&rule, SparseVector<double>())).first;
    SparseVector<double>& f = it->second;
    string prev = "<r>";
    for (int i = 0; i < rule.f_.size(); ++i) {
      WordID w = rule.f_[i];
      if (w < 0) w = -w;
      assert(w > 0);
      const string& cur = TD::Convert(w);
      ostringstream os;
      os << "RBS:" << prev << '_' << cur;
      const int fid = FD::Convert(Escape(os.str()));
      if (fid <= 0) return;
      f.add_value(fid, 1.0);
      prev = cur;
    }
    ostringstream os;
    os << "RBS:" << prev << '_' << "</r>";
    f.set_value(FD::Convert(Escape(os.str())), 1.0);
  }
  (*features) += it->second;
}

RuleTargetBigramFeatures::RuleTargetBigramFeatures(const std::string& param) : inds(1000) {
  for (unsigned i = 0; i < inds.size(); ++i) {
    ostringstream os;
    os << (i + 1);
    inds[i] = os.str();
  }
}

void RuleTargetBigramFeatures::PrepareForInput(const SentenceMetadata& smeta) {
  rule2_feats_.clear();
}

void RuleTargetBigramFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                         const Hypergraph::Edge& edge,
                                         const vector<const void*>& ant_contexts,
                                         SparseVector<double>* features,
                                         SparseVector<double>* estimated_features,
                                         void* context) const {
  map<const TRule*, SparseVector<double> >::iterator it = rule2_feats_.find(edge.rule_.get());
  if (it == rule2_feats_.end()) {
    const TRule& rule = *edge.rule_;
    it = rule2_feats_.insert(make_pair(&rule, SparseVector<double>())).first;
    SparseVector<double>& f = it->second;
    string prev = "<r>";
    vector<WordID> nt_types(rule.Arity());
    unsigned ntc = 0;
    for (int i = 0; i < rule.f_.size(); ++i)
      if (rule.f_[i] < 0) nt_types[ntc++] = -rule.f_[i];
    for (int i = 0; i < rule.e_.size(); ++i) {
      WordID w = rule.e_[i];
      string cur;
      if (w > 0) {
        cur = TD::Convert(w);
      } else {
        cur = TD::Convert(nt_types[-w]) + inds[-w];
      }
      ostringstream os;
      os << "RBT:" << prev << '_' << cur;
      const int fid = FD::Convert(Escape(os.str()));
      if (fid <= 0) return;
      f.add_value(fid, 1.0);
      prev = cur;
    }
    ostringstream os;
    os << "RBT:" << prev << '_' << "</r>";
    f.set_value(FD::Convert(Escape(os.str())), 1.0);
  }
  (*features) += it->second;
}

