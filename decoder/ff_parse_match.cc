#include "ff_parse_match.h"

#include <sstream>
#include <stack>
#include <string>

#include "sentence_metadata.h"
#include "array2d.h"
#include "filelib.h"

using namespace std;

// implements the parse match features as described in Vilar et al. (2008)
// source trees must be represented in Penn Treebank format, e.g.
//     (S (NP John) (VP (V left)))

struct ParseMatchFeaturesImpl {
  ParseMatchFeaturesImpl(const string& param) {
    if (param.compare("") != 0) {
      char score_param = (char) param[0];
      switch(score_param) {
        case 'b':
          scoring_method = 0;
          break;
        case 'l':
          scoring_method = 1;
          break;
        case 'e':
          scoring_method = 2;
          break;
        case 'r':
          scoring_method = 3;
          break;
        default:
          scoring_method = 0;
      }
    }
    else {
      scoring_method = 0;
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
    src_sent_len = src_len;
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
    //cerr << "i=" << i << "  src_len=" << src_len << endl;
    }
    //cerr << "i=" << i << "  src_len=" << src_len << endl;
    assert(i == src_len);  // make sure tree specified in src_tree is
                           // the same length as the source sentence
  }

  int FireFeatures(const TRule& rule, const int i, const int j, int* ants, SparseVector<double>* feats) {
    //cerr << "fire features: " << rule.AsString() << " for " << i << "," << j << endl;
    //cerr << rule << endl;
    //cerr << "span: " << i << " " << j << endl;
    const WordID lhs = src_tree(i,j);
    int fid_ef = FD::Convert("PM");
    int min_dist; // minimal distance to next syntactic constituent of this rule's LHS
    int summed_min_dists; // minimal distances of LHS and NTs summed up
    if (TD::Convert(lhs).compare("XX") != 0)
        min_dist= 0;
    // compute the distance to the next syntactical constituent
    else {
      int ok = 0;
      for (unsigned int k = 1; k < (j - i); k++) {
        min_dist = k;
        for (unsigned int l = 0; l <= k; l++) {
          // check if adding k words to the rule span will
          // lead to a syntactical constituent
          int l_add = i-l;
          int r_add = j+(k-l);
          //cerr << "Adding: " << l_add << " " << r_add << endl;
          if ((l_add < src_tree.width() && r_add < src_tree.height()) && (TD::Convert(src_tree(l_add, r_add)).compare("XX") != 0)) {
            //cerr << TD::Convert(src_tree(i-l,j+(k-l))) << endl;
            //cerr << "span_add: " << l_add << " " << r_add << endl;
            ok = 1;
            break;
          }
          // check if removing k words from the rule span will
          // lead to a syntactical constituent
          else {
            //cerr << "Hilfe...!" << endl;
            int l_rem= i+l;
            int r_rem = j-(k-l);
            //cerr << "Removing: " << l_rem << " " << r_rem << endl;
            if ((l_rem < src_tree.width() && r_rem < src_tree.height()) && TD::Convert(src_tree(l_rem, r_rem)).compare("XX") != 0) {
              //cerr << TD::Convert(src_tree(i+l,j-(k-l))) << endl;
              //cerr << "span_rem: " << l_rem << " " << r_rem << endl;
              ok = 1;
              break;
            }
          }
        }
        if (ok) break;
      }
    }
    summed_min_dists = min_dist;
    //cerr << min_dist << endl;
    unsigned ntc = 0;
    for (unsigned k = 0; k < rule.f_.size(); ++k) {
      int fj = rule.f_[k];
      if (fj <= 0)
        summed_min_dists += ants[ntc++];
    }
    switch(scoring_method) {
      case 0:
        // binary scoring
        feats->set_value(fid_ef, (summed_min_dists == 0));
        break;
      // CHECK: for the remaining scoring methods, the question remains if
      // min_dist or summed_min_dists should be used
      case 1:
        // linear scoring
        feats->set_value(fid_ef, 1.0/(min_dist+1));
        break;
      case 2:
        // exponential scoring
        feats->set_value(fid_ef, 1.0/exp(min_dist));
        break;
      case 3:
        // relative scoring
        feats->set_value(fid_ef, (j-i)/((j-i) + min_dist));
        break;
      default:
        // binary scoring in case nothing is defined
        feats->set_value(fid_ef, (summed_min_dists == 0));
    }
    return min_dist;
  }

  Array2D<WordID> src_tree; // src_tree(i,j) NT = type
  unsigned int src_sent_len;
  mutable Array2D<map<const TRule*, int> > fids_ef; // fires for fully lexicalized
  int scoring_method;
};

ParseMatchFeatures::ParseMatchFeatures(const string& param) :
    FeatureFunction(sizeof(WordID)) {
  impl = new ParseMatchFeaturesImpl(param);
}

ParseMatchFeatures::~ParseMatchFeatures() {
  delete impl;
  impl = NULL;
}

void ParseMatchFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  int ants[8];
  for (unsigned i = 0; i < ant_contexts.size(); ++i)
    ants[i] = *static_cast<const int*>(ant_contexts[i]);

  *static_cast<int*>(context) =
     impl->FireFeatures(*edge.rule_, edge.i_, edge.j_, ants, features);
}

void ParseMatchFeatures::PrepareForInput(const SentenceMetadata& smeta) {
  ReadFile f = ReadFile(smeta.GetSGMLValue("src_tree"));
  string tree;
  f.ReadAll(tree);
  impl->InitializeGrids(tree, smeta.GetSourceLength());
}

