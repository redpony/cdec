#include "ff_ruleshape.h"

#include "trule.h"
#include "hg.h"
#include "fdict.h"
#include <sstream>

using namespace std;

inline bool IsBitSet(int i, int bit) {
  const int mask = 1 << bit;
  return (i & mask);
}

inline char BitAsChar(bool bit) {
  return (bit ? '1' : '0');
}

RuleShapeFeatures::RuleShapeFeatures(const string& /* param */) {
  bool first = true;
  for (int i = 0; i < 32; ++i) {
    for (int j = 0; j < 32; ++j) {
      ostringstream os;
      os << "Shape_S";
      Node* cur = &fidtree_;
      for (int k = 0; k < 5; ++k) {
        bool bit = IsBitSet(i,k);
        cur = &cur->next_[bit];
        os << BitAsChar(bit);
      }
      os << "_T";
      for (int k = 0; k < 5; ++k) {
        bool bit = IsBitSet(j,k);
        cur = &cur->next_[bit];
        os << BitAsChar(bit);
      }
      if (first) { first = false; cerr << "  Example feature: " << os.str() << endl; }
      cur->fid_ = FD::Convert(os.str());
    }
  }
}

void RuleShapeFeatures::TraversalFeaturesImpl(const SentenceMetadata& /* smeta */,
                                              const Hypergraph::Edge& edge,
                                              const vector<const void*>& /* ant_contexts */,
                                              SparseVector<double>* features,
                                              SparseVector<double>* /* estimated_features */,
                                              void* /* context */) const {
  const Node* cur = &fidtree_;
  TRule& rule = *edge.rule_;
  int pos = 0;  // feature position
  int i = 0;
  while(i < rule.f_.size()) {
    WordID sym = rule.f_[i];
    if (pos % 2 == 0) {
      if (sym > 0) {       // is terminal
        cur = Advance(cur, true);
        while (i < rule.f_.size() && rule.f_[i] > 0) ++i;  // consume lexical string
      } else {
        cur = Advance(cur, false);
      }
      ++pos;
    } else {  // expecting a NT
      if (sym < 1) {
        cur = Advance(cur, true);
        ++i;
        ++pos;
      } else {
        cerr << "BAD RULE: " << rule.AsString() << endl;
        exit(1);
      }
    }
  }
  for (; pos < 5; ++pos)
    cur = Advance(cur, false);
  assert(pos == 5);  // this will fail if you are using using > binary rules!

  i = 0;
  while(i < rule.e_.size()) {
    WordID sym = rule.e_[i];
    if (pos % 2 == 1) {
      if (sym > 0) {       // is terminal
        cur = Advance(cur, true);
        while (i < rule.e_.size() && rule.e_[i] > 0) ++i;  // consume lexical string
      } else {
        cur = Advance(cur, false);
      }
      ++pos;
    } else {  // expecting a NT
      if (sym < 1) {
        cur = Advance(cur, true);
        ++i;
        ++pos;
      } else {
        cerr << "BAD RULE: " << rule.AsString() << endl;
        exit(1);
      }
    }
  }
  for (;pos < 10; ++pos)
    cur = Advance(cur, false);
  assert(pos == 10);  // this will fail if you are using using > binary rules!

  features->set_value(cur->fid_, 1.0);
}

