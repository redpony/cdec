#include "translator.h"

#include <sstream>
#include <boost/shared_ptr.hpp>

#include "sentence_metadata.h"
#include "filelib.h"
#include "hg.h"
#include "hg_io.h"
#include "earley_composer.h"
#include "phrasetable_fst.h"
#include "tdict.h"

using namespace std;

struct FSTTranslatorImpl {
  FSTTranslatorImpl(const boost::program_options::variables_map& conf) :
      goal_sym(conf["goal"].as<string>()),
      kGOAL_RULE(new TRule("[Goal] ||| [" + goal_sym + ",1] ||| [1]")),
      kGOAL(TD::Convert("Goal") * -1),
      add_pass_through_rules(conf.count("add_pass_through_rules")) {
    fst.reset(LoadTextPhrasetable(conf["grammar"].as<vector<string> >()));
    ec.reset(new EarleyComposer(fst.get()));
  }

  bool Translate(const string& input,
                 const vector<double>& weights,
                 Hypergraph* forest) {
    bool composed = false;
    if (input.find("{\"rules\"") == 0) {
      istringstream is(input);
      Hypergraph src_cfg_hg;
      if (!HypergraphIO::ReadFromJSON(&is, &src_cfg_hg)) {
        cerr << "Failed to read HG from JSON.\n";
        abort();
      }
      if (add_pass_through_rules) {
        SparseVector<double> feats;
        feats.set_value(FD::Convert("PassThrough"), 1);
        for (int i = 0; i < src_cfg_hg.edges_.size(); ++i) {
          const vector<WordID>& f = src_cfg_hg.edges_[i].rule_->f_;
          for (int j = 0; j < f.size(); ++j) {
            if (f[j] > 0) {
              fst->AddPassThroughTranslation(f[j], feats);
            }
          }
        }
      }
      composed = ec->Compose(src_cfg_hg, forest);
    } else {
      const string dummy_grammar("[" + goal_sym + "] ||| " + input + " ||| TOP=1");
      cerr << "  Dummy grammar: " << dummy_grammar << endl;
      istringstream is(dummy_grammar);
      if (add_pass_through_rules) {
        vector<WordID> words;
        TD::ConvertSentence(input, &words);
        SparseVector<double> feats;
        feats.set_value(FD::Convert("PassThrough"), 1);
        for (int i = 0; i < words.size(); ++i)
          fst->AddPassThroughTranslation(words[i], feats);
      }
      composed = ec->Compose(&is, forest);
    }
    if (composed) {
      Hypergraph::TailNodeVector tail(1, forest->nodes_.size() - 1);
      Hypergraph::Node* goal = forest->AddNode(TD::Convert("Goal")*-1);
      Hypergraph::Edge* hg_edge = forest->AddEdge(kGOAL_RULE, tail);
      forest->ConnectEdgeToHeadNode(hg_edge, goal);
      forest->Reweight(weights);
    }
    if (add_pass_through_rules)
      fst->ClearPassThroughTranslations();
    return composed;
  }

  const string goal_sym;
  const TRulePtr kGOAL_RULE;
  const WordID kGOAL;
  const bool add_pass_through_rules;
  boost::shared_ptr<EarleyComposer> ec;
  boost::shared_ptr<FSTNode> fst;
};

FSTTranslator::FSTTranslator(const boost::program_options::variables_map& conf) :
  pimpl_(new FSTTranslatorImpl(conf)) {}

bool FSTTranslator::TranslateImpl(const string& input,
                              SentenceMetadata* smeta,
                              const vector<double>& weights,
                              Hypergraph* minus_lm_forest) {
  smeta->SetSourceLength(0);  // don't know how to compute this
  return pimpl_->Translate(input, weights, minus_lm_forest);
}

