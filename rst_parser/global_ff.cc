#include "global_ff.h"

#include <iostream>

#include "tdict.h"

using namespace std;

struct GFFImpl {
  void PrepareForInput(const TaggedSentence& sentence) {
  }
  void Features(const TaggedSentence& sentence,
                const EdgeSubset& tree,
                SparseVector<double>* feats) const {
    const vector<WordID>& words = sentence.words;
    const vector<WordID>& tags = sentence.pos;
    const vector<pair<short,short> >& hms = tree.h_m_pairs;
    assert(words.size() == tags.size());
    vector<int> mods(words.size());
    for (int i = 0; i < hms.size(); ++i) {
      mods[hms[i].first]++;        // first = head, second = modifier
    }
    for (int i = 0; i < mods.size(); ++i) {
      ostringstream os;
      os << "NM:" << TD::Convert(tags[i]) << "_" << mods[i];
      feats->add_value(FD::Convert(os.str()), 1.0);
    }
  }
};

GlobalFeatureFunctions::GlobalFeatureFunctions() {}
GlobalFeatureFunctions::~GlobalFeatureFunctions() { delete pimpl; }

void GlobalFeatureFunctions::PrepareForInput(const TaggedSentence& sentence) {
  pimpl->PrepareForInput(sentence);
}

void GlobalFeatureFunctions::Features(const TaggedSentence& sentence,
                                      const EdgeSubset& tree,
                                      SparseVector<double>* feats) const {
  pimpl->Features(sentence, tree, feats);
}

