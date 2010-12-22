#include "csplit.h"

#include <iostream>

#include "filelib.h"
#include "stringlib.h"
#include "hg.h"
#include "tdict.h"
#include "grammar.h"
#include "sentence_metadata.h"

using namespace std;

struct CompoundSplitImpl {
  CompoundSplitImpl(const boost::program_options::variables_map& conf) :
      fugen_elements_(true),
      min_size_(3),
      kXCAT(TD::Convert("X")*-1),
      kWORDBREAK_RULE(new TRule("[X] ||| # ||| #")),
      kTEMPLATE_RULE(new TRule("[X] ||| [X,1] ? ||| [1] ?")),
      kGOAL_RULE(new TRule("[Goal] ||| [X,1] ||| [1]")),
      kFUGEN_S(FD::Convert("FugS")),
      kFUGEN_N(FD::Convert("FugN")) {
    // TODO: use conf to turn fugenelements on and off
  }

  void PasteTogetherStrings(const vector<string>& chars,
                            const int i,
                            const int j,
                            string* yield) {
    int size = 0;
    for (int k=i; k<j; ++k)
      size += chars[k].size();
    yield->resize(size);
    int cur = 0;
    for (int k=i; k<j; ++k) {
      const string& cs = chars[k];
      for (int l = 0; l < cs.size(); ++l)
        (*yield)[cur++] = cs[l];
    }
  }

  void BuildTrellis(const vector<string>& chars,
                    Hypergraph* forest) {
    vector<int> nodes(chars.size()+1, -1);
    nodes[0] = forest->AddNode(kXCAT)->id_;       // source
    const int left_rule = forest->AddEdge(kWORDBREAK_RULE, Hypergraph::TailNodeVector())->id_;
    forest->ConnectEdgeToHeadNode(left_rule, nodes[0]);

    const int max_split_ = max(static_cast<int>(chars.size()) - min_size_ + 1, 1);
    // cerr << "max: " << max_split_ << "  " << " min: " << min_size_ << endl;
    for (int i = min_size_; i < max_split_; ++i)
      nodes[i] = forest->AddNode(kXCAT)->id_;
    assert(nodes.back() == -1);
    nodes.back() = forest->AddNode(kXCAT)->id_;   // sink

    for (int i = 0; i < max_split_; ++i) {
      if (nodes[i] < 0) continue;
      const int start = min(i + min_size_, static_cast<int>(chars.size()));
      for (int j = start; j <= chars.size(); ++j) {
        if (nodes[j] < 0) continue;
        string yield;
        PasteTogetherStrings(chars, i, j, &yield);
        // cerr << "[" << i << "," << j << "] " << yield << endl;
        TRulePtr rule = TRulePtr(new TRule(*kTEMPLATE_RULE));
        rule->e_[1] = rule->f_[1] = TD::Convert(yield);
        // cerr << rule->AsString() << endl;
        int edge = forest->AddEdge(
          rule,
          Hypergraph::TailNodeVector(1, nodes[i]))->id_;
        forest->ConnectEdgeToHeadNode(edge, nodes[j]);
        forest->edges_[edge].i_ = i;
        forest->edges_[edge].j_ = j;

        // handle "fugenelemente" here
        // don't delete "fugenelemente" at the end of words
        if (fugen_elements_ && j != chars.size()) {
          const int len = yield.size();
          string alt;
          int fid = 0;
          if (len > (min_size_ + 2) && yield[len-1] == 's' && yield[len-2] == 'e') {
            alt = yield.substr(0, len - 2);
            fid = kFUGEN_S;
          } else if (len > (min_size_ + 1) && yield[len-1] == 's') {
            alt = yield.substr(0, len - 1);
            fid = kFUGEN_S;
          } else if (len > (min_size_ + 2) && yield[len-2] == 'e' && yield[len-1] == 'n') {
            alt = yield.substr(0, len - 1);
            fid = kFUGEN_N;
          }
          if (alt.size()) {
            TRulePtr altrule = TRulePtr(new TRule(*rule));
            altrule->e_[1] = TD::Convert(alt);
            // cerr << altrule->AsString() << endl;
            int edge = forest->AddEdge(
              altrule,
              Hypergraph::TailNodeVector(1, nodes[i]))->id_;
            forest->ConnectEdgeToHeadNode(edge, nodes[j]);
            forest->edges_[edge].feature_values_.set_value(fid, 1.0);
            forest->edges_[edge].i_ = i;
            forest->edges_[edge].j_ = j;
          }
        }
      }
    }

    // add goal rule
    Hypergraph::TailNodeVector tail(1, forest->nodes_.size() - 1);
    Hypergraph::Node* goal = forest->AddNode(TD::Convert("Goal")*-1);
    Hypergraph::Edge* hg_edge = forest->AddEdge(kGOAL_RULE, tail);
    forest->ConnectEdgeToHeadNode(hg_edge, goal);
  }
 private:
  const bool fugen_elements_;
  const int min_size_;
  const WordID kXCAT;
  const TRulePtr kWORDBREAK_RULE;
  const TRulePtr kTEMPLATE_RULE;
  const TRulePtr kGOAL_RULE;
  const int kFUGEN_S;
  const int kFUGEN_N;
};

CompoundSplit::CompoundSplit(const boost::program_options::variables_map& conf) :
  pimpl_(new CompoundSplitImpl(conf)) {}

static void SplitUTF8String(const string& in, vector<string>* out) {
  out->resize(in.size());
  int i = 0;
  int c = 0;
  while (i < in.size()) {
    const int len = UTF8Len(in[i]);
    assert(len);
    (*out)[c] = in.substr(i, len);
    ++c;
    i += len;
  }
  out->resize(c);
}

bool CompoundSplit::TranslateImpl(const string& input,
                      SentenceMetadata* smeta,
                      const vector<double>& weights,
                      Hypergraph* forest) {
  if (input.find(" ") != string::npos) {
    cerr << "  BAD INPUT: " << input << "\n    CompoundSplit expects single words\n";
    abort();
  }
  vector<string> in;
  SplitUTF8String(input, &in);
  smeta->SetSourceLength(in.size());  // TODO do utf8 or somethign
  for (int i = 0; i < in.size(); ++i)
    smeta->src_lattice_.push_back(vector<LatticeArc>(1, LatticeArc(TD::Convert(in[i]), 0.0, 1)));
  pimpl_->BuildTrellis(in, forest);
  forest->Reweight(weights);
  return true;
}

int CompoundSplit::GetFullWordEdgeIndex(const Hypergraph& forest) {
  assert(forest.nodes_.size() > 0);
  const vector<int> out_edges = forest.nodes_[0].out_edges_;
  int max_edge = -1;
  int max_j = -1;
  for (int i = 0; i < out_edges.size(); ++i) {
    const int j = forest.edges_[out_edges[i]].j_;
    if (j > max_j) {
      max_j = j;
      max_edge = out_edges[i];
    }
  }
  assert(max_edge >= 0);
  assert(max_edge < forest.edges_.size());
  return max_edge;
}

