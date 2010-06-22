#include "translator.h"

#include <vector>

#include "hg.h"
#include "grammar.h"
#include "bottom_up_parser.h"
#include "sentence_metadata.h"

using namespace std;
static bool usingSentenceGrammar = false;
static bool printGrammarsUsed = false;

struct SCFGTranslatorImpl {
  SCFGTranslatorImpl(const boost::program_options::variables_map& conf) :
      max_span_limit(conf["scfg_max_span_limit"].as<int>()),
      add_pass_through_rules(conf.count("add_pass_through_rules")),
      goal(conf["goal"].as<string>()),
      default_nt(conf["scfg_default_nt"].as<string>()) {
    if(conf.count("grammar"))
      {
	vector<string> gfiles = conf["grammar"].as<vector<string> >();
	for (int i = 0; i < gfiles.size(); ++i) {
	  cerr << "Reading SCFG grammar from " << gfiles[i] << endl;
	  TextGrammar* g = new TextGrammar(gfiles[i]);
	  g->SetMaxSpan(max_span_limit);
	  g->SetGrammarName(gfiles[i]);
	  grammars.push_back(GrammarPtr(g));
	  
	}
      }
    if (!conf.count("scfg_no_hiero_glue_grammar"))
      { 
	GlueGrammar* g = new GlueGrammar(goal, default_nt);
	g->SetGrammarName("GlueGrammar");
	grammars.push_back(GrammarPtr(g));
	cerr << "Adding glue grammar" << endl;
      }
    if (conf.count("scfg_extra_glue_grammar"))
      {
	GlueGrammar* g = new GlueGrammar(conf["scfg_extra_glue_grammar"].as<string>());
	g->SetGrammarName("ExtraGlueGrammar");		
	grammars.push_back(GrammarPtr(g));
	cerr << "Adding extra glue grammar" << endl;
      }
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
    Lattice& lattice = smeta->src_lattice_;
    LatticeTools::ConvertTextOrPLF(input, &lattice);
    smeta->SetSourceLength(lattice.size());
    if (add_pass_through_rules){
      PassThroughGrammar* g = new PassThroughGrammar(lattice, default_nt);
      g->SetGrammarName("PassThrough");
      glist.push_back(GrammarPtr(g));
      cerr << "Adding pass through grammar" << endl;
    }



    if(printGrammarsUsed){    //Iterate trough grammars we have for this sentence and list them
      for (int gi = 0; gi < glist.size(); ++gi) 
	{
	  cerr << "Using grammar::" << 	 glist[gi]->GetGrammarName() << endl;
	}
    }

    ExhaustiveBottomUpParser parser(goal, glist);
    if (!parser.Parse(lattice, forest))
      return false;
    forest->Reweight(weights);
    return true;
  }
};

/*
Called once from cdec.cc to setup the initial SCFG translation structure backend
*/
SCFGTranslator::SCFGTranslator(const boost::program_options::variables_map& conf) :
  pimpl_(new SCFGTranslatorImpl(conf)) {}

/*
Called for each sentence to perform translation using the SCFG backend
*/
bool SCFGTranslator::TranslateImpl(const string& input,
                               SentenceMetadata* smeta,
                               const vector<double>& weights,
                               Hypergraph* minus_lm_forest) {
  
  return pimpl_->Translate(input, smeta, weights, minus_lm_forest);
}

/*
Check for grammar pointer in the sentence markup, for use with sentence specific grammars
 */
void SCFGTranslator::ProcessMarkupHintsImpl(const map<string, string>& kv) {
  map<string,string>::const_iterator it = kv.find("grammar");
 
  
  if (it == kv.end()) {
    usingSentenceGrammar= false; 
    return;
  }
  //Create sentence specific grammar from specified file name and load grammar into list of grammars
  cerr << "Loading sentence grammar from:" << it->second <<  endl;
  usingSentenceGrammar = true;
  TextGrammar* sentGrammar = new TextGrammar(it->second);
  sentGrammar->SetMaxSpan(pimpl_->max_span_limit);
  sentGrammar->SetGrammarName(it->second);
  pimpl_->grammars.push_back(GrammarPtr(sentGrammar));

}

void SCFGTranslator::SentenceCompleteImpl() {

  if(usingSentenceGrammar)      // Drop the last sentence grammar from the list of grammars
    {
      cerr << "Clearing grammar" << endl;
      pimpl_->grammars.pop_back();
    }
}

