#include "ff_source_syntax.h"

#include <sstream>
#include <stack>
#ifndef HAVE_OLD_CPP
# include <unordered_set>
#else
# include <tr1/unordered_set>
namespace std { using std::tr1::unordered_set; }
#endif

#include "sentence_metadata.h"
#include "array2d.h"
#include "filelib.h"

using namespace std;

// implements the source side syntax features described in Blunsom et al. (EMNLP 2008)
// source trees must be represented in Penn Treebank format, e.g.
//     (S (NP John) (VP (V left)))

// log transform to make long spans cluster together
// but preserve differences
inline int SpanSizeTransform(unsigned span_size) {
  if (!span_size) return 0;
  return static_cast<int>(log(span_size+1) / log(1.39)) - 1;
}

struct SourceSyntaxFeaturesImpl {
  SourceSyntaxFeaturesImpl() {}

  SourceSyntaxFeaturesImpl(const string& param) {
    if (!(param.compare("") == 0)) {
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
    //fids_cat.clear();
    fids_ef.clear();
    src_tree.clear();
    //fids_cat.resize(src_len, src_len + 1);
    fids_ef.resize(src_len, src_len + 1);
    src_tree.resize(src_len, src_len + 1, TD::Convert("XX"));
    ParseTreeString(tree, src_len);
  }

  void ParseTreeString(const string& tree, unsigned src_len) {
    stack<pair<int, WordID> > stk;  // first = i, second = category
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
          // cur_cat.second spans from cur_cat.first to i
          // cerr << TD::Convert(cur_cat.second) << " from " << cur_cat.first << " to " << i << endl;
          // NOTE: unary rule chains end up being labeled with the top-most category
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
    }
    // cerr << "i=" << i << "  src_len=" << src_len << endl;
    assert(i == src_len);  // make sure tree specified in src_tree is
                           // the same length as the source sentence
  }

  WordID FireFeatures(const TRule& rule, const int i, const int j, const WordID* ants, SparseVector<double>* feats) {
    //cerr << "fire features: " << rule.AsString() << " for " << i << "," << j << endl;
    const WordID lhs = src_tree(i,j);
    //int& fid_cat = fids_cat(i,j);
    int& fid_ef = fids_ef(i,j)[&rule];
    if (fid_ef <= 0) {
      ostringstream os;
      //ostringstream os2;
      os << "SSYN:" << TD::Convert(lhs);
      //os2 << "SYN:" << TD::Convert(lhs) << '_' << SpanSizeTransform(j - i);
      //fid_cat = FD::Convert(os2.str());
      os << ':';
      unsigned ntc = 0;
      for (unsigned k = 0; k < rule.f_.size(); ++k) {
        if (k > 0) os << '_';
        int fj = rule.f_[k];
        if (fj <= 0) {
          os << '[' << TD::Convert(ants[ntc++]) << ']';
        } else {
          os << TD::Convert(fj);
        }
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
    }
    if (fid_ef > 0) {
      if (feature_filter.size()>0) {
        if (feature_filter.find(fid_ef) != feature_filter.end()) {
          feats->set_value(fid_ef, 1.0);
        }
      } else {
        feats->set_value(fid_ef, 1.0);
      }
    }
    cerr << FD::Convert(fid_ef) << endl;
    return lhs;
  }

  Array2D<WordID> src_tree; // src_tree(i,j) NT = type
  // mutable Array2D<int> fids_cat; // this tends to overfit baddly
  mutable Array2D<map<const TRule*, int> > fids_ef; // fires for fully lexicalized
  unordered_set<int> feature_filter;
};

SourceSyntaxFeatures::SourceSyntaxFeatures(const string& param) :
    FeatureFunction(sizeof(WordID)) {
  impl = new SourceSyntaxFeaturesImpl(param);
}

SourceSyntaxFeatures::~SourceSyntaxFeatures() {
  delete impl;
  impl = NULL;
}

void SourceSyntaxFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
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

void SourceSyntaxFeatures::PrepareForInput(const SentenceMetadata& smeta) {
  ReadFile f = ReadFile(smeta.GetSGMLValue("src_tree"));
  string tree;
  f.ReadAll(tree);
  impl->InitializeGrids(tree, smeta.GetSourceLength());
}

struct SourceSpanSizeFeaturesImpl {
  SourceSpanSizeFeaturesImpl() {}

  void InitializeGrids(unsigned src_len) {
    fids.clear();
    fids.resize(src_len, src_len + 1);
  }

  int FireFeatures(const TRule& rule, const int i, const int j, const WordID* ants, SparseVector<double>* feats) {
    if (rule.Arity() > 0) {
      int& fid = fids(i,j)[&rule];
      if (fid <= 0) {
        ostringstream os;
        os << "SSS:";
        unsigned ntc = 0;
        for (unsigned k = 0; k < rule.f_.size(); ++k) {
          if (k > 0) os << '_';
          int fj = rule.f_[k];
          if (fj <= 0) {
            os << '[' << TD::Convert(-fj) << ants[ntc++] << ']';
          } else {
            os << TD::Convert(fj);
          }
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
        fid = FD::Convert(os.str());
      }
      if (fid > 0)
        feats->set_value(fid, 1.0);
    }
    return SpanSizeTransform(j - i);
  }

  mutable Array2D<map<const TRule*, int> > fids;
};

SourceSpanSizeFeatures::SourceSpanSizeFeatures(const string& param) :
    FeatureFunction(sizeof(char)) {
  impl = new SourceSpanSizeFeaturesImpl;
}

SourceSpanSizeFeatures::~SourceSpanSizeFeatures() {
  delete impl;
  impl = NULL;
}

void SourceSpanSizeFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  int ants[8];
  for (unsigned i = 0; i < ant_contexts.size(); ++i)
    ants[i] = *static_cast<const char*>(ant_contexts[i]);

  *static_cast<char*>(context) =
     impl->FireFeatures(*edge.rule_, edge.i_, edge.j_, ants, features);
}

void SourceSpanSizeFeatures::PrepareForInput(const SentenceMetadata& smeta) {
  impl->InitializeGrids(smeta.GetSourceLength());
}

