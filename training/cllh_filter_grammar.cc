#include <iostream>
#include <vector>
#include <cassert>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "tdict.h"
#include "ff_register.h"
#include "verbose.h"
#include "hg.h"
#include "decoder.h"
#include "filelib.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("training_data,t",po::value<string>(),"Training data corpus")
        ("decoder_config,c",po::value<string>(),"Decoder configuration file");
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

  if (conf->count("help") || !conf->count("training_data") || !conf->count("decoder_config")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

void ReadTrainingCorpus(const string& fname, vector<string>* c) {
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  while(in) {
    getline(in, line);
    if (!in) break;
    c->push_back(line);
  }
}

struct TrainingObserver : public DecoderObserver {
  TrainingObserver() : s_lhs(-TD::Convert("S")), goal_lhs(-TD::Convert("Goal")) {}

  void Reset() {
    total_complete = 0;
  } 

  virtual void NotifyDecodingStart(const SentenceMetadata& smeta) {
    state = 1;
    used.clear();
    failed = true;
  }

  virtual void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    assert(state == 1);
    for (int i = 0; i < hg->edges_.size(); ++i) {
      const TRulePtr& rule = hg->edges_[i].rule_;
      if (rule->lhs_ == s_lhs || rule->lhs_ == goal_lhs)  // fragile hack to filter out glue rules
        continue;
      if (rule->prev_i == -1)
        used.insert(rule);
    }
    state = 2;
  }

  virtual void NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    assert(state == 2);
    state = 3;
  }

  virtual void NotifyDecodingComplete(const SentenceMetadata& smeta) {
    if (state == 3) {
      failed = false;
    } else {
      failed = true;
    }
  }

  set<TRulePtr> used;

  const int s_lhs;
  const int goal_lhs;
  bool failed;
  int total_complete;
  int state;
};

int main(int argc, char** argv) {
  SetSilent(true);  // turn off verbose decoder output
  register_feature_functions();

  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());
  if (decoder.GetConf()["input"].as<string>() != "-") {
    cerr << "cdec.ini must not set an input file\n";
    abort();
  }

  vector<string> corpus;
  ReadTrainingCorpus(conf["training_data"].as<string>(), &corpus);
  assert(corpus.size() > 0);

  TrainingObserver observer;
  for (int i = 0; i < corpus.size(); ++i) {
    int ex_num = i;
    decoder.SetId(ex_num);
    decoder.Decode(corpus[ex_num], &observer);
    if (observer.failed) {
      cerr << "*** id=" << ex_num << " is unreachable\n";
      observer.used.clear();
    } else {
      cerr << corpus[ex_num] << endl;
      for (set<TRulePtr>::iterator it = observer.used.begin(); it != observer.used.end(); ++it) {
        cout << **it << endl;
        (*it)->prev_i = 0;
      }
    }
  }
  return 0;
}
