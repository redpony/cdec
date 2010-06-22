#include "ff_tagger.h"

#include "tdict.h"
#include "sentence_metadata.h"

#include <sstream>

using namespace std;

Tagger_BigramIdentity::Tagger_BigramIdentity(const std::string& param) :
  FeatureFunction(sizeof(WordID)) {}

void Tagger_BigramIdentity::FireFeature(const WordID& left,
                                 const WordID& right,
                                 SparseVector<double>* features) const {
  int& fid = fmap_[left][right];
  if (!fid) {
    ostringstream os;
    if (right == 0) {
      os << "Uni:" << TD::Convert(left);
    } else {
      os << "Bi:";
      if (left < 0) { os << "BOS"; } else { os << TD::Convert(left); }
      os << '_';
      if (right < 0) { os << "EOS"; } else { os << TD::Convert(right); }
    }
    fid = FD::Convert(os.str());
  }
  features->set_value(fid, 1.0);
}

void Tagger_BigramIdentity::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  WordID& out_context = *static_cast<WordID*>(context);
  const int arity = edge.Arity();
  if (arity == 0) {
    out_context = edge.rule_->e_[0];
    FireFeature(out_context, 0, features);
  } else if (arity == 2) {
    WordID left = *static_cast<const WordID*>(ant_contexts[0]);
    WordID right = *static_cast<const WordID*>(ant_contexts[1]);
    if (edge.i_ == 0 && edge.j_ == 2)
      FireFeature(-1, left, features);
    FireFeature(left, right, features);
    if (edge.i_ == 0 && edge.j_ == smeta.GetSourceLength())
      FireFeature(right, -1, features);
    out_context = right;
  }
}

LexicalPairIdentity::LexicalPairIdentity(const std::string& param) {}

void LexicalPairIdentity::FireFeature(WordID src,
                                 WordID trg,
                                 SparseVector<double>* features) const {
  int& fid = fmap_[src][trg];
  if (!fid) {
    static map<WordID, WordID> escape;
    if (escape.empty()) {
      escape[TD::Convert("=")] = TD::Convert("__EQ");
      escape[TD::Convert(";")] = TD::Convert("__SC");
      escape[TD::Convert(",")] = TD::Convert("__CO");
    }
    if (escape.count(src)) src = escape[src];
    if (escape.count(trg)) trg = escape[trg];
    ostringstream os;
    os << "Id:" << TD::Convert(src) << ':' << TD::Convert(trg);
    fid = FD::Convert(os.str());
  }
  features->set_value(fid, 1.0);
}

void LexicalPairIdentity::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  const vector<WordID>& ew = edge.rule_->e_;
  const vector<WordID>& fw = edge.rule_->f_;
  for (int i = 0; i < ew.size(); ++i) {
    const WordID& e = ew[i];
    if (e <= 0) continue;
    for (int j = 0; j < fw.size(); ++j) {
      const WordID& f = fw[j];
      if (f <= 0) continue;
      FireFeature(f, e, features);
    }
  }
}


