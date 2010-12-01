#include "lextrans.h"

#include <iostream>
#include <cstdlib>

#include "filelib.h"
#include "hg.h"
#include "tdict.h"
#include "grammar.h"
#include "sentence_metadata.h"

using namespace std;

struct LexicalTransImpl {
  LexicalTransImpl(const boost::program_options::variables_map& conf) :
      use_null(conf.count("lextrans_use_null") > 0),
      align_only_(conf.count("lextrans_align_only") > 0),
      psg_file_(),
      kXCAT(TD::Convert("X")*-1),
      kNULL(TD::Convert("<eps>")),
      kBINARY(new TRule("[X] ||| [X,1] [X,2] ||| [1] [2]")),
      kGOAL_RULE(new TRule("[Goal] ||| [X,1] ||| [1]")) {
    if (conf.count("per_sentence_grammar_file")) {
      psg_file_ = new ifstream(conf["per_sentence_grammar_file"].as<string>().c_str());
    }
    vector<string> gfiles = conf["grammar"].as<vector<string> >();
    assert(gfiles.size() == 1);
    ReadFile rf(gfiles.front());
    TextGrammar *tg = new TextGrammar;
    grammar.reset(tg);
    istream* in = rf.stream();
    int lc = 0;
    bool flag = false;
    string line;
    while(*in) {
      getline(*in, line);
      if (!*in) continue;
      ++lc;
      TRulePtr r(TRule::CreateRulePhrasetable(line));
      tg->AddRule(r);
      if (lc %   50000 == 0) { cerr << '.'; flag = true; }
      if (lc % 2000000 == 0) { cerr << " [" << lc << "]\n"; flag = false; }
    }
    if (flag) cerr << endl;
    cerr << "Loaded " << lc << " rules\n";
  }

  void LoadSentenceGrammar(const string& s_offset) {
    const unsigned long long int offset = strtoull(s_offset.c_str(), NULL, 10);
    psg_file_->seekg(offset, ios::beg);
    TextGrammar *tg = new TextGrammar;
    sup_grammar.reset(tg);
    const string kEND_MARKER = "###EOS###";
    string line;
    while(true) {
      assert(*psg_file_);
      getline(*psg_file_, line);
      if (line == kEND_MARKER) break;
      TRulePtr r(TRule::CreateRulePhrasetable(line));
      tg->AddRule(r);
    }
  }

  bool BuildTrellis(const Lattice& lattice, const SentenceMetadata& smeta, Hypergraph* forest) {
    if (psg_file_) {
      const string offset = smeta.GetSGMLValue("psg");
      if (offset.size() < 2 || offset[0] != '@') {
        cerr << "per_sentence_grammar_file given but sentence id=" << smeta.GetSentenceID() << " doesn't have grammar info!\n";
        abort();
      }
      LoadSentenceGrammar(offset.substr(1));
    }
    const int e_len = smeta.GetTargetLength();
    assert(e_len > 0);
    const int f_len = lattice.size();
    // hack to tell the feature function system how big the sentence pair is
    const int f_start = (use_null ? -1 : 0);
    int prev_node_id = -1;
    set<WordID> target_vocab;
    const Lattice& ref = smeta.GetReference();
    for (int i = 0; i < ref.size(); ++i) {
      target_vocab.insert(ref[i][0].label);
    }
    bool all_sources_to_all_targets_ = true;
    set<WordID> trgs_used;
    for (int i = 0; i < e_len; ++i) {  // for each word in the *target*
      Hypergraph::Node* node = forest->AddNode(kXCAT);
      const int new_node_id = node->id_;
      for (int j = f_start; j < f_len; ++j) { // for each word in the source
        const WordID src_sym = (j < 0 ? kNULL : lattice[j][0].label);
        const GrammarIter* gi = grammar->GetRoot()->Extend(src_sym);
        if (!gi) {
          if (psg_file_)
            gi = sup_grammar->GetRoot()->Extend(src_sym);
          if (!gi) {
            cerr << "No translations found for: " << TD::Convert(src_sym) << "\n";
            return false;
          }
        }
        const RuleBin* rb = gi->GetRules();
        assert(rb);
        for (int k = 0; k < rb->GetNumRules(); ++k) {
          TRulePtr rule = rb->GetIthRule(k);
          const WordID trg_word = rule->e_[0];
          if (align_only_) {
            if (target_vocab.count(trg_word) == 0)
              continue;
          }
          if (all_sources_to_all_targets_ && (target_vocab.count(trg_word) > 0))
            trgs_used.insert(trg_word);
          Hypergraph::Edge* edge = forest->AddEdge(rule, Hypergraph::TailNodeVector());
          edge->i_ = j;
          edge->j_ = j+1;
          edge->prev_i_ = i;
          edge->prev_j_ = i+1;
          edge->feature_values_ += edge->rule_->GetFeatureValues();
          forest->ConnectEdgeToHeadNode(edge->id_, new_node_id);
        }
        if (all_sources_to_all_targets_) {
          for (set<WordID>::iterator it = target_vocab.begin(); it != target_vocab.end(); ++it) {
            if (trgs_used.count(*it)) continue;
            const WordID ungenerated_trg_word = *it;
            TRulePtr rule;
            rule.reset(TRule::CreateLexicalRule(src_sym, ungenerated_trg_word));
            Hypergraph::Edge* edge = forest->AddEdge(rule, Hypergraph::TailNodeVector());
            edge->i_ = j;
            edge->j_ = j+1;
            edge->prev_i_ = i;
            edge->prev_j_ = i+1;
            forest->ConnectEdgeToHeadNode(edge->id_, new_node_id);
          }
          trgs_used.clear();
        }
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
    return true;
  }

 private:
  const bool use_null;
  const bool align_only_;
  ifstream* psg_file_;
  const WordID kXCAT;
  const WordID kNULL;
  const TRulePtr kBINARY;
  const TRulePtr kGOAL_RULE;
  GrammarPtr grammar;
  GrammarPtr sup_grammar;
};

LexicalTrans::LexicalTrans(const boost::program_options::variables_map& conf) :
  pimpl_(new LexicalTransImpl(conf)) {}

bool LexicalTrans::TranslateImpl(const string& input,
                      SentenceMetadata* smeta,
                      const vector<double>& weights,
                      Hypergraph* forest) {
  Lattice& lattice = smeta->src_lattice_;
  LatticeTools::ConvertTextOrPLF(input, &lattice);
  if (!lattice.IsSentence()) {
    // lexical models make independence assumptions
    // that don't work with lattices or conf nets
    cerr << "LexicalTrans: cannot deal with lattice source input!\n";
    abort();
  }
  smeta->SetSourceLength(lattice.size());
  if (!pimpl_->BuildTrellis(lattice, *smeta, forest)) return false;
  forest->is_linear_chain_ = true;
  forest->Reweight(weights);
  return true;
}

