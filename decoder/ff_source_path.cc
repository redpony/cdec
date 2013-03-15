#include "ff_source_path.h"

#include "hg.h"

using namespace std;

SourcePathFeatures::SourcePathFeatures(const string& param) : FeatureFunction(sizeof(int)) {}

void SourcePathFeatures::FireBigramFeature(WordID prev, WordID cur, SparseVector<double>* features) const {
  int& fid = bigram_fids[prev][cur];
  if (!fid) fid = FD::Convert("SB:"+TD::Convert(prev) + "_" + TD::Convert(cur));
  if (fid) features->add_value(fid, 1.0);
}

void SourcePathFeatures::FireUnigramFeature(WordID cur, SparseVector<double>* features) const {
  int& fid = unigram_fids[cur];
  if (!fid) fid = FD::Convert("SU:" + TD::Convert(cur));
  if (fid) features->add_value(fid, 1.0);
}

void SourcePathFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                               const HG::Edge& edge,
                                               const vector<const void*>& ant_contexts,
                                               SparseVector<double>* features,
                                               SparseVector<double>* estimated_features,
                                               void* context) const {
  WordID* res = reinterpret_cast<WordID*>(context);
  const vector<int>& f = edge.rule_->f();
  int prev = 0;
  unsigned ntc = 0;
  for (unsigned i = 0; i < f.size(); ++i) {
    int cur = f[i];
    if (cur < 0)
      cur = *reinterpret_cast<const WordID*>(ant_contexts[ntc++]);
    else
      FireUnigramFeature(cur, features);
    if (prev) FireBigramFeature(prev, cur, features);
    prev = cur;
  }
  *res = prev;
}

