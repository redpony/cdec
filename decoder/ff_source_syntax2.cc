#include "ff_source_syntax2.h"

#include <sstream>
#include <stack>
#include <string>

#include "sentence_metadata.h"
#include "array2d.h"
#include "filelib.h"

using namespace std;

// implements the source side syntax features described in Blunsom et al. (EMNLP 2008)
// source trees must be represented in Penn Treebank format, e.g.
//     (S (NP John) (VP (V left)))

struct SourceSyntaxFeatures2Impl {
  SourceSyntaxFeatures2Impl(const string& param) {
    if (param.compare("") != 0) {
      string triggered_features_fn = param;
      ReadFile triggered_features(triggered_features_fn);
      string in;
      while(getline(*triggered_features, in)) {
        feature_filter.insert(FD::Convert(in));
      }
    }
  }

  void InitializeGrids(const string& tree, unsigned src_len) {
    assert(tree.size() > 0);
    fids_ef.clear();
    src_tree.clear();
    fids_ef.resize(src_len, src_len + 1);
    src_tree.resize(src_len, src_len + 1, TD::Convert("XX"));
    ParseTreeString(tree, src_len);
  }

  void ParseTreeString(const string& tree, unsigned src_len) {
    //cerr << "TREE: " << tree << endl;
    stack<pair<int, WordID> > stk; // first = i, second = category
    pair<int, WordID> cur_cat; cur_cat.first = -1;
    unsigned i = 0;
    unsigned p = 0;
    while(p < tree.size()) {
      const char cur = tree[p];
      if (cur == '(') {
        stk.push(cur_cat);
        ++p;
        unsigned k = p + 1;
        while (k < tree.size() && tree[k] != ' ') { ++k; }
        cur_cat.first = i;
        cur_cat.second = TD::Convert(tree.substr(p, k - p));
        // cerr << "NT: '" << tree.substr(p, k-p) << "' (i=" << i << ")\n";
        p = k + 1;
      } else if (cur == ')') {
        unsigned k = p;
        while (k < tree.size() && tree[k] == ')') { ++k; }
        const unsigned num_closes = k - p;
        for (unsigned ci = 0; ci < num_closes; ++ci) {
          src_tree(cur_cat.first, i) = cur_cat.second;
          cur_cat = stk.top();
          stk.pop();
        }
        p = k;
        while (p < tree.size() && (tree[p] == ' ' || tree[p] == '\t')) { ++p; }
      } else if (cur == ' ' || cur == '\t') {
        cerr << "Unexpected whitespace in: " << tree << endl;
        abort();
      } else { // terminal symbol
        unsigned k = p + 1;
        do {
          while (k < tree.size() && tree[k] != ')' && tree[k] != ' ') { ++k; }
          // cerr << "TERM: '" << tree.substr(p, k-p) << "' (i=" << i << ")\n";
          ++i;
          assert(i <= src_len);
          while (k < tree.size() && tree[k] == ' ') { ++k; }
          p = k;
        } while (p < tree.size() && tree[p] != ')');
      }
    //cerr << "i=" << i << "  src_len=" << src_len << endl;
    }
    //cerr << "i=" << i << "  src_len=" << src_len << endl;
    assert(i == src_len);  // make sure tree specified in src_tree is
                           // the same length as the source sentence
  }

  WordID FireFeatures(const TRule& rule, const int i, const int j, const WordID* ants, SparseVector<double>* feats) {
    //cerr << "fire features: " << rule.AsString() << " for " << i << "," << j << endl;
    const WordID lhs = src_tree(i,j);
    int& fid_ef = fids_ef(i,j)[&rule];
    ostringstream os;
    os << "SSYN2:" << TD::Convert(lhs);
    os << ':';
    unsigned ntc = 0;
    for (unsigned k = 0; k < rule.f_.size(); ++k) {
      int fj = rule.f_[k];
      if (k > 0 && fj <= 0) os << '_';
      if (fj <= 0) {
        os << '[' << TD::Convert(ants[ntc++]) << ']';
      }/*else {
        os << TD::Convert(fj);
      }*/
    }
    os << ':';
    for (unsigned k = 0; k < rule.e_.size(); ++k) {
      const int ei = rule.e_[k];
      if (k > 0) os << '_';
      if (ei <= 0)
        os << '[' << (1-ei) << ']';
      else
        os << TD::Convert(ei);
    }
    fid_ef = FD::Convert(os.str());
    //cerr << "FEATURE: " << os.str() << endl;
    //cerr << "FID_EF: " << fid_ef << endl;
    if (feature_filter.size() > 0) {
      if (feature_filter.find(fid_ef) != feature_filter.end()) {
        //cerr << "SYN-Feature was trigger more than once on training set." << endl;
        feats->set_value(fid_ef, 1.0);
      }
      //else cerr << "SYN-Feature was triggered less than once on training set." << endli;
    }
    else {
      feats->set_value(fid_ef, 1.0);
    }
    cerr << FD::Convert(fid_ef) << endl;
    return lhs;
  }

  Array2D<WordID> src_tree; // src_tree(i,j) NT = type
  mutable Array2D<map<const TRule*, int> > fids_ef; // fires for fully lexicalized
  unordered_set<int> feature_filter;
};

SourceSyntaxFeatures2::SourceSyntaxFeatures2(const string& param) :
    FeatureFunction(sizeof(WordID)) {
  impl = new SourceSyntaxFeatures2Impl(param);
}

SourceSyntaxFeatures2::~SourceSyntaxFeatures2() {
  delete impl;
  impl = NULL;
}

void SourceSyntaxFeatures2::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  WordID ants[8];
  for (unsigned i = 0; i < ant_contexts.size(); ++i)
    ants[i] = *static_cast<const WordID*>(ant_contexts[i]);

  *static_cast<WordID*>(context) =
     impl->FireFeatures(*edge.rule_, edge.i_, edge.j_, ants, features);
}

void SourceSyntaxFeatures2::PrepareForInput(const SentenceMetadata& smeta) {
  ReadFile f = ReadFile(smeta.GetSGMLValue("src_tree"));
  string tree;
  f.ReadAll(tree);
  impl->InitializeGrids(tree, smeta.GetSourceLength());
}

