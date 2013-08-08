#include "ff_soft_syntax.h"

#include <cstdio>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

#include "sentence_metadata.h"
#include "stringlib.h"
#include "array2d.h"
#include "filelib.h"

using namespace std;

// Implements the soft syntactic features described in 
// Marton and Resnik (2008): "Soft Syntacitc Constraints for Hierarchical Phrase-Based Translation".
// Source trees must be represented in Penn Treebank format,
// e.g. (S (NP John) (VP (V left))).

struct SoftSyntacticFeaturesImpl {
  SoftSyntacticFeaturesImpl(const string& param) {
    vector<string> labels = SplitOnWhitespace(param);
	for (unsigned int i = 0; i < labels.size(); i++) 
      //cerr << "Labels: " << labels.at(i) << endl;
    for (unsigned int i = 0; i < labels.size(); i++) {
      string label = labels.at(i);
      pair<string, string> feat_label;
      feat_label.first = label.substr(0, label.size() - 1);
      feat_label.second = label.at(label.size() - 1);
      feat_labels.push_back(feat_label);
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
    //cerr << "String " << tree << endl;
    while(p < tree.size()) {
      const char cur = tree[p];
      if (cur == '(') {
        stk.push(cur_cat);
        ++p;
        unsigned k = p + 1;
        while (k < tree.size() && tree[k] != ' ') { ++k; }
        cur_cat.first = i;
        cur_cat.second = TD::Convert(tree.substr(p, k - p));
        //cerr << "NT: '" << tree.substr(p, k-p) << "' (i=" << i << ")\n";
        p = k + 1;
      } else if (cur == ')') {
        unsigned k = p;
        while (k < tree.size() && tree[k] == ')') { ++k; }
        const unsigned num_closes = k - p;
        for (unsigned ci = 0; ci < num_closes; ++ci) {
          // cur_cat.second spans from cur_cat.first to i
          //cerr << TD::Convert(cur_cat.second) << " from " << cur_cat.first << " to " << i << endl;
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
    //cerr << "i=" << i << "  src_len=" << src_len << endl;
    assert(i == src_len);  // make sure tree specified in src_tree is
                           // the same length as the source sentence
  }

  WordID FireFeatures(const TRule& rule, const int i, const int j, const WordID* ants, SparseVector<double>* feats) {
    //cerr << "fire features: " << rule.AsString() << " for " << i << "," << j << endl;
    const WordID lhs = src_tree(i,j);
	string lhs_str = TD::Convert(lhs);
    //cerr << "LHS: " << lhs_str << " from " << i << " to " << j << endl;
	//cerr << "RULE :"<< rule << endl;
    int& fid_ef = fids_ef(i,j)[&rule];
    for (unsigned int i = 0; i < feat_labels.size(); i++) {
      ostringstream os;
      string label = feat_labels.at(i).first;
      //cerr << "This Label: " << label << endl;
      char feat_type = (char) feat_labels.at(i).second.c_str()[0];
      //cerr << "feat_type: " << feat_type << endl;
      switch(feat_type) {
        case '2':
          if (lhs_str.compare(label) == 0) {
            os << "SYN:" << label << "_conform";
          }
          else {
            os << "SYN:" << label << "_cross";
          }
          fid_ef = FD::Convert(os.str());
          if (fid_ef > 0) {
            //cerr << "Feature :" << os.str() << endl;
            feats->set_value(fid_ef, 1.0);
          }
          break;
        case '_':
          os << "SYN:" << label;
          fid_ef = FD::Convert(os.str());
          if (lhs_str.compare(label) == 0) {
            if (fid_ef > 0) {
            //cerr << "Feature: " << os.str() << endl;
              feats->set_value(fid_ef, 1.0);
            }
          }
          else {
            if (fid_ef > 0) {
              //cerr << "Feature: " << os.str() << endl;
              feats->set_value(fid_ef, -1.0);
            }
          }
          break;
        case '+':
          if (lhs_str.compare(label) == 0) {
            os << "SYN:" << label << "_conform";
            fid_ef = FD::Convert(os.str());
            if (fid_ef > 0) {
              //cerr << "Feature: " << os.str() << endl;
              feats->set_value(fid_ef, 1.0);
            }
          }
          break;
        case '-': 
          //cerr << "-" << endl;           
          if (lhs_str.compare(label) != 0) {
            os << "SYN:" << label << "_cross";
            fid_ef = FD::Convert(os.str());
            if (fid_ef > 0) {
              //cerr << "Feature :" << os.str() << endl;
              feats->set_value(fid_ef, 1.0);
            }
          }
          break;
        os.clear();
        os.str("");
      }
      //cerr << "Feature: " << os.str() << endl;
      //cerr << endl;
    }
    return lhs;
  }

  Array2D<WordID> src_tree;  // src_tree(i,j) NT = type
  mutable Array2D<map<const TRule*, int> > fids_ef;    // fires for fully lexicalized
  vector<pair<string, string> > feat_labels;
};

SoftSyntacticFeatures::SoftSyntacticFeatures(const string& param) :
    FeatureFunction(sizeof(WordID)) {
  impl = new SoftSyntacticFeaturesImpl(param);
}

SoftSyntacticFeatures::~SoftSyntacticFeatures() {
  delete impl;
  impl = NULL;
}

void SoftSyntacticFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
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

void SoftSyntacticFeatures::PrepareForInput(const SentenceMetadata& smeta) {
  impl->InitializeGrids(smeta.GetSGMLValue("src_tree"), smeta.GetSourceLength());
}
