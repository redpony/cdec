#include "translator.h"

#include <sstream>
#include <boost/shared_ptr.hpp>

#include "filelib.h"
#include "sentence_metadata.h"
#include "hg.h"
#include "hg_io.h"
#include "tdict.h"

using namespace std;

struct RescoreTranslatorImpl {
  RescoreTranslatorImpl(const boost::program_options::variables_map& conf) :
      goal_sym(conf["goal"].as<string>()),
      kGOAL_RULE(new TRule("[Goal] ||| [" + goal_sym + ",1] ||| [1]")),
      kGOAL(TD::Convert("Goal") * -1) {
  }

  bool Translate(const string& input,
                 const vector<double>& weights,
                 Hypergraph* forest) {
    istringstream is(input);
    string header, fname;
    is >> header >> fname;
    if (header != "::forest::") {
      cerr << "RescoreTranslator: expected input lines of form ::forest:: filename.gz\n";
      abort();
    }
    ReadFile rf(fname);
    if (!rf) { cerr << "Can't read " << fname << endl; abort(); }
    Hypergraph src_cfg_hg;
    if (!HypergraphIO::ReadFromBinary(rf.stream(), forest)) {
      cerr << "Parse error while reading HG.\n";
      abort();
    }
    Hypergraph::TailNodeVector tail(1, forest->nodes_.size() - 1);
    Hypergraph::Node* goal = forest->AddNode(kGOAL);
    Hypergraph::Edge* hg_edge = forest->AddEdge(kGOAL_RULE, tail);
    forest->ConnectEdgeToHeadNode(hg_edge, goal);
    forest->Reweight(weights);
    return true;
  }

  const string goal_sym;
  const TRulePtr kGOAL_RULE;
  const WordID kGOAL;
};

RescoreTranslator::RescoreTranslator(const boost::program_options::variables_map& conf) :
  pimpl_(new RescoreTranslatorImpl(conf)) {}

bool RescoreTranslator::TranslateImpl(const string& input,
                              SentenceMetadata* smeta,
                              const vector<double>& weights,
                              Hypergraph* minus_lm_forest) {
  smeta->SetSourceLength(0);  // don't know how to compute this
  smeta->input_type_ = cdec::kFOREST;
  return pimpl_->Translate(input, weights, minus_lm_forest);
}

