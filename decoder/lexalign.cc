#include "lexalign.h"

#include <iostream>

#include "filelib.h"
#include "hg.h"
#include "tdict.h"
#include "grammar.h"
#include "sentence_metadata.h"

using namespace std;

struct LexicalAlignImpl {
  LexicalAlignImpl(const boost::program_options::variables_map& conf) :
      use_null(conf.count("lexcrf_use_null") > 0),
      kXCAT(TD::Convert("X")*-1),
      kNULL(TD::Convert("<eps>")),
      kBINARY(new TRule("[X] ||| [X,1] [X,2] ||| [1] [2]")),
      kGOAL_RULE(new TRule("[Goal] ||| [X,1] ||| [1]")) {
  }

  void BuildTrellis(const Lattice& lattice, const SentenceMetadata& smeta, Hypergraph* forest) {
    const int e_len = smeta.GetTargetLength();
    assert(e_len > 0);
    const Lattice& target = smeta.GetReference();
    const int f_len = lattice.size();
    // hack to tell the feature function system how big the sentence pair is
    const int f_start = (use_null ? -1 : 0);
    int prev_node_id = -1;
    for (int i = 0; i < e_len; ++i) {  // for each word in the *target*
      const WordID& e_i = target[i][0].label;
      Hypergraph::Node* node = forest->AddNode(kXCAT);
      const int new_node_id = node->id_;
      for (int j = f_start; j < f_len; ++j) { // for each word in the source
        const WordID src_sym = (j < 0 ? kNULL : lattice[j][0].label);
        TRulePtr& rule = LexRule(src_sym, e_i);
        Hypergraph::Edge* edge = forest->AddEdge(rule, Hypergraph::TailNodeVector());
        edge->i_ = j;
        edge->j_ = j+1;
        edge->prev_i_ = i;
        edge->prev_j_ = i+1;
        edge->feature_values_ += edge->rule_->GetFeatureValues();
        forest->ConnectEdgeToHeadNode(edge->id_, new_node_id);
      }
      if (prev_node_id >= 0) {
        const int comb_node_id = forest->AddNode(kXCAT)->id_;
        Hypergraph::TailNodeVector tail(2, prev_node_id);
        tail[1] = new_node_id;
        Hypergraph::Edge* edge = forest->AddEdge(kBINARY, tail);
        forest->ConnectEdgeToHeadNode(edge->id_, comb_node_id);
        prev_node_id = comb_node_id;
      } else {
        prev_node_id = new_node_id;
      }
    }
    Hypergraph::TailNodeVector tail(1, forest->nodes_.size() - 1);
    Hypergraph::Node* goal = forest->AddNode(TD::Convert("Goal")*-1);
    Hypergraph::Edge* hg_edge = forest->AddEdge(kGOAL_RULE, tail);
    forest->ConnectEdgeToHeadNode(hg_edge, goal);
  }

  inline int LexFeatureId(const WordID& f, const WordID& e) {
    map<int, int>& e2fid = f2e2fid[f];
    map<int, int>::iterator it = e2fid.find(e);
    if (it != e2fid.end())
      return it->second;
    int& fid = e2fid[e];
    if (f == 0) {
      fid = FD::Convert("Lx_<eps>_" + FD::Escape(TD::Convert(e)));
    } else {
      fid = FD::Convert("Lx_" + FD::Escape(TD::Convert(f)) + "_" + FD::Escape(TD::Convert(e)));
    }
    return fid;
  }

  inline TRulePtr& LexRule(const WordID& f, const WordID& e) {
    map<int, TRulePtr>& e2rule = f2e2rule[f];
    map<int, TRulePtr>::iterator it = e2rule.find(e);
    if (it != e2rule.end())
      return it->second;
    TRulePtr& tr = e2rule[e];
    tr.reset(TRule::CreateLexicalRule(f, e));
    tr->scores_.set_value(LexFeatureId(f, e), 1.0);
    return tr;
  }

 private:
  const bool use_null;
  const WordID kXCAT;
  const WordID kNULL;
  const TRulePtr kBINARY;
  const TRulePtr kGOAL_RULE;
  map<int, map<int, TRulePtr> > f2e2rule;
  map<int, map<int, int> > f2e2fid;
  GrammarPtr grammar;
};

LexicalAlign::LexicalAlign(const boost::program_options::variables_map& conf) :
  pimpl_(new LexicalAlignImpl(conf)) {}

bool LexicalAlign::Translate(const string& input,
                      SentenceMetadata* smeta,
                      const vector<double>& weights,
                      Hypergraph* forest) {
  Lattice lattice;
  LatticeTools::ConvertTextToLattice(input, &lattice);
  smeta->SetSourceLength(lattice.size());
  pimpl_->BuildTrellis(lattice, *smeta, forest);
  forest->is_linear_chain_ = true;
  forest->Reweight(weights);
  return true;
}

