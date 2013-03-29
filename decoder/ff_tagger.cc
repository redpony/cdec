#include "ff_tagger.h"

#include <sstream>

#include "hg.h"
#include "tdict.h"
#include "sentence_metadata.h"
#include "stringlib.h"

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

Tagger_BigramIndicator::Tagger_BigramIndicator(const std::string& param) :
  FeatureFunction(sizeof(WordID)) {
   no_uni_ = (LowercaseString(param) == "no_uni");
}

void Tagger_BigramIndicator::FireFeature(const WordID& left,
                                 const WordID& right,
                                 SparseVector<double>* features) const {
  if (no_uni_ && right == 0) return;
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
    fid = FD::Convert(Escape(os.str()));
  }
  features->set_value(fid, 1.0);
}

void Tagger_BigramIndicator::TraversalFeaturesImpl(const SentenceMetadata& smeta,
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
  } else if (arity == 1) {
    out_context = *static_cast<const WordID*>(ant_contexts[0]);
  } else if (arity == 2) {
    WordID left = *static_cast<const WordID*>(ant_contexts[0]);
    WordID right = *static_cast<const WordID*>(ant_contexts[1]);
    if (edge.i_ == 0 && edge.j_ == 2)
      FireFeature(-1, left, features);
    FireFeature(left, right, features);
    if (edge.i_ == 0 && edge.j_ == smeta.GetSourceLength())
      FireFeature(right, -1, features);
    out_context = right;
  } else {
    assert(!"shouldn't happen");
  }
}

void LexicalPairIndicator::PrepareForInput(const SentenceMetadata& smeta) {
  lexmap_->PrepareForInput(smeta);
}

LexicalPairIndicator::LexicalPairIndicator(const std::string& param) {
  name_ = "Id";
  if (param.size()) {
    // name corpus.f emap.txt
    vector<string> params;
    SplitOnWhitespace(param, &params);
    if (params.size() != 3) {
      cerr << "LexicalPairIndicator takes 3 parameters: <name> <corpus.src.txt> <trgmap.txt>\n";
      cerr << " * may be used for corpus.src.txt or trgmap.txt to use surface forms\n";
      cerr << " Received: " << param << endl;
      abort();
    }
    name_ = params[0];
    lexmap_.reset(new FactoredLexiconHelper(params[1], params[2]));
  } else {
    lexmap_.reset(new FactoredLexiconHelper);
  }
}

void LexicalPairIndicator::FireFeature(WordID src,
                                      WordID trg,
                                      SparseVector<double>* features) const {
  int& fid = fmap_[src][trg];
  if (!fid) {
    ostringstream os;
    os << name_ << ':' << TD::Convert(src) << ':' << TD::Convert(trg);
    fid = FD::Convert(Escape(os.str()));
  }
  features->set_value(fid, 1.0);
}

void LexicalPairIndicator::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  // inline WordID SourceWordAtPosition(const int i);
  // inline WordID CoarsenedTargetWordForTarget(const WordID surface_target);
  if (edge.Arity() == 0) {
    const WordID src = lexmap_->SourceWordAtPosition(edge.i_);
    const vector<WordID>& ew = edge.rule_->e_;
    assert(ew.size() == 1);
    const WordID trg = lexmap_->CoarsenedTargetWordForTarget(ew[0]);
    FireFeature(src, trg, features);
  }
}

OutputIndicator::OutputIndicator(const std::string& param) {}

void OutputIndicator::FireFeature(WordID trg,
                                 SparseVector<double>* features) const {
  int& fid = fmap_[trg];
  if (!fid) {
    static map<WordID, WordID> escape;
    if (escape.empty()) {
      escape[TD::Convert("=")] = TD::Convert("__EQ");
      escape[TD::Convert(";")] = TD::Convert("__SC");
      escape[TD::Convert(",")] = TD::Convert("__CO");
    }
    if (escape.count(trg)) trg = escape[trg];
    ostringstream os;
    os << "T:" << TD::Convert(trg);
    fid = FD::Convert(Escape(os.str()));
  }
  features->set_value(fid, 1.0);
}

void OutputIndicator::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  const vector<WordID>& ew = edge.rule_->e_;
  for (int i = 0; i < ew.size(); ++i) {
    const WordID& e = ew[i];
    if (e > 0) FireFeature(e, features);
  }
}



