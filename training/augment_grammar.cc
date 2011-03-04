#include <iostream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "weights.h"
#include "rule_lexer.h"
#include "trule.h"
#include "filelib.h"
#include "tdict.h"
#include "lm/model.hh"
#include "lm/enumerate_vocab.hh"
#include "wordid.h"

namespace po = boost::program_options;
using namespace std;

vector<lm::WordIndex> word_map;
lm::ngram::ProbingModel* ngram;
struct VMapper : public lm::ngram::EnumerateVocab {
  VMapper(vector<lm::WordIndex>* out) : out_(out), kLM_UNKNOWN_TOKEN(0) { out_->clear(); }
  void Add(lm::WordIndex index, const StringPiece &str) {
    const WordID cdec_id = TD::Convert(str.as_string());
    if (cdec_id >= out_->size())
      out_->resize(cdec_id + 1, kLM_UNKNOWN_TOKEN);
    (*out_)[cdec_id] = index;
  }
  vector<lm::WordIndex>* out_;
  const lm::WordIndex kLM_UNKNOWN_TOKEN;
};

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("source_lm,l",po::value<string>(),"Source language LM (KLM)")
        ("collapse_weights,w",po::value<string>(), "Collapse weights into a single feature X using the coefficients from this weights file")
        ("add_shape_types,s", "Add rule shape types");
  po::options_description clo("Command line options");
  clo.add_options()
        ("config", po::value<string>(), "Configuration file")
        ("help,h", "Print this help message and exit");
  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts);
  dcmdline_options.add(opts).add(clo);
  
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("config")) {
    ifstream config((*conf)["config"].as<string>().c_str());
    po::store(po::parse_config_file(config, dconfig_options), *conf);
  }
  po::notify(*conf);

  if (conf->count("help")) {
    cerr << "Usage " << argv[0] << " [OPTIONS]\n";
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

lm::WordIndex kSOS;

template <class Model> float Score(const vector<WordID>& str, const Model &model) {
  typename Model::State state, out;
  lm::FullScoreReturn ret;
  float total = 0.0f;
  state = model.NullContextState();

  for (int i = 0; i < str.size(); ++i) {
    lm::WordIndex vocab = ((str[i] < word_map.size() && str[i] > 0) ? word_map[str[i]] : 0);
    if (vocab == kSOS) {
      state = model.BeginSentenceState();
    } else {
      ret = model.FullScore(state, vocab, out);
      total += ret.prob;
      state = out;
    }
  }
  return total;
}

int kSrcLM;
vector<double> col_weights;

static void RuleHelper(const TRulePtr& new_rule, const unsigned int ctf_level, const TRulePtr& coarse_rule, void* extra) {
  static const int kSrcLM = FD::Convert("SrcLM");
  static const int kPC = FD::Convert("PC");
  static const int kX = FD::Convert("X");
  TRule r(*new_rule);
  if (ngram) r.scores_.set_value(kSrcLM, Score(r.f_, *ngram));
  r.scores_.set_value(kPC, 1.0);
  if (col_weights.size()) {
    double score = r.scores_.dot(col_weights);
    r.scores_.clear();
    r.scores_.set_value(kX, score);
  }
  cout << r << endl;
}


int main(int argc, char** argv) {
  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf)) return 1;
  if (conf.count("source_lm")) {
    lm::ngram::Config kconf;
    VMapper vm(&word_map);
    kconf.enumerate_vocab = &vm; 
    ngram = new lm::ngram::ProbingModel(conf["source_lm"].as<string>().c_str(), kconf);
    kSOS = word_map[TD::Convert("<s>")];
    cerr << "Loaded " << (int)ngram->Order() << "-gram KenLM (MapSize=" << word_map.size() << ")\n";
    cerr << "  <s> = " << kSOS << endl;
  } else { ngram = NULL; }
  if (conf.count("collapse_weights")) {
    Weights w;
    w.InitFromFile(conf["collapse_weights"].as<string>());
    w.InitVector(&col_weights);
  }
  RuleLexer::ReadRules(&cin, &RuleHelper, NULL);
  return 0;
}

