#include "translator.h"

#include <vector>

#include "hg.h"
#include "grammar.h"
#include "bottom_up_parser.h"
#include "sentence_metadata.h"

using namespace std;

Translator::~Translator() {}

struct SCFGTranslatorImpl {
  SCFGTranslatorImpl(const boost::program_options::variables_map& conf) :
      max_span_limit(conf["scfg_max_span_limit"].as<int>()),
      add_pass_through_rules(conf.count("add_pass_through_rules")),
      goal(conf["goal"].as<string>()),
      default_nt(conf["scfg_default_nt"].as<string>()) {
    vector<string> gfiles = conf["grammar"].as<vector<string> >();
    for (int i = 0; i < gfiles.size(); ++i) {
      cerr << "Reading SCFG grammar from " << gfiles[i] << endl;
      TextGrammar* g = new TextGrammar(gfiles[i]);
      g->SetMaxSpan(max_span_limit);
      grammars.push_back(GrammarPtr(g));
    }
    if (!conf.count("scfg_no_hiero_glue_grammar"))
      grammars.push_back(GrammarPtr(new GlueGrammar(goal, default_nt)));
    if (conf.count("scfg_extra_glue_grammar"))
      grammars.push_back(GrammarPtr(new GlueGrammar(conf["scfg_extra_glue_grammar"].as<string>())));
  }

  const int max_span_limit;
  const bool add_pass_through_rules;
  const string goal;
  const string default_nt;
  vector<GrammarPtr> grammars;

  bool Translate(const string& input,
                 SentenceMetadata* smeta,
                 const vector<double>& weights,
                 Hypergraph* forest) {
    vector<GrammarPtr> glist = grammars;
    Lattice lattice;
    LatticeTools::ConvertTextOrPLF(input, &lattice);
    smeta->SetSourceLength(lattice.size());
    if (add_pass_through_rules)
      glist.push_back(GrammarPtr(new PassThroughGrammar(lattice, default_nt)));
    ExhaustiveBottomUpParser parser(goal, glist);
    if (!parser.Parse(lattice, forest))
      return false;
    forest->Reweight(weights);
    return true;
  }
};

SCFGTranslator::SCFGTranslator(const boost::program_options::variables_map& conf) :
  pimpl_(new SCFGTranslatorImpl(conf)) {}

bool SCFGTranslator::Translate(const string& input,
                               SentenceMetadata* smeta,
                               const vector<double>& weights,
                               Hypergraph* minus_lm_forest) {
  return pimpl_->Translate(input, smeta, weights, minus_lm_forest);
}

