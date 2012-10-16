#include "ff_charset.h"

#include "tdict.h"
#include "hg.h"
#include "fdict.h"
#include "stringlib.h"

using namespace std;

NonLatinCount::NonLatinCount(const string& param) : FeatureFunction(), fid_(FD::Convert("NonLatinCount")) {}

bool ContainsNonLatin(const string& word) {
  unsigned cur = 0;
  while(cur < word.size()) {
    const int size = UTF8Len(word[cur]);
    if (size > 1) return true;
    cur += size;  
  }
  return false;
}

void NonLatinCount::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                          const Hypergraph::Edge& edge,
                                          const std::vector<const void*>& ant_contexts,
                                          SparseVector<double>* features,
                                          SparseVector<double>* estimated_features,
                                          void* context) const {
  const vector<WordID>& e = edge.rule_->e();
  int count = 0;
  for (int i = 0; i < e.size(); ++i) {
    if (e[i] > 0) {
      map<WordID, bool>::iterator it = is_non_latin_.find(e[i]);
      if (it == is_non_latin_.end()) {
        if ((is_non_latin_[e[i]] = ContainsNonLatin(TD::Convert(e[i]))))
          ++count;
      } else {
        if (it->second)
          ++count;
      }
    }
  }
  if (count) features->set_value(fid_, count);
}

