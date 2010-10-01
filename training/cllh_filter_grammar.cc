#include <iostream>
#include <vector>
#include <cassert>
#include <unistd.h>   // fork
#include <sys/wait.h> // waitpid

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
        ("decoder_config,c",po::value<string>(),"Decoder configuration file")
        ("shards,s",po::value<unsigned>()->default_value(1),"Number of shards")
        ("starting_shard,S",po::value<unsigned>()->default_value(0), "In this invocation only process shards >= S")
        ("work_limit,l",po::value<unsigned>()->default_value(9999), "Process maximially this many shards")
        ("ncpus,C",po::value<unsigned>()->default_value(1),"Number of CPUs to use");
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

void ReadTrainingCorpus(const string& fname, int rank, int size, vector<string>* c, vector<int>* ids) {
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  int lc = 0;
  assert(size > 0);
  assert(rank < size);
  while(in) {
    getline(in, line);
    if (!in) break;
    if (lc % size == rank) {
      c->push_back(line);
      ids->push_back(lc);
    }
    ++lc;
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
      const TRule* rule = hg->edges_[i].rule_.get();
      if (rule->lhs_ == s_lhs || rule->lhs_ == goal_lhs)  // fragile hack to filter out glue rules
        continue;
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

  set<const TRule*> used;

  const int s_lhs;
  const int goal_lhs;
  bool failed;
  int total_complete;
  int state;
};

void work(const string& fname, int rank, int size, Decoder* decoder) {
  cerr << "Worker " << rank << '/' << size << " starting.\n";
  vector<string> corpus;
  vector<int> ids;
  ReadTrainingCorpus(fname, rank, size, &corpus, &ids);
  assert(corpus.size() > 0);
  assert(corpus.size() == ids.size());
  cerr << "  " << rank << '/' << size << ": has " << corpus.size() << " sentences to process\n";
  ostringstream oc; oc << "corpus." << rank << "_of_" << size;
  WriteFile foc(oc.str());
  ostringstream og; og << "grammar." << rank << "_of_" << size << ".gz";
  WriteFile fog(og.str());

  set<const TRule*> all_used;
  TrainingObserver observer;
  for (int i = 0; i < corpus.size(); ++i) {
    const int sent_id = ids[i];
    const string& input = corpus[i];
    decoder->SetId(sent_id);
    decoder->Decode(input, &observer);
    if (observer.failed) {
      // do nothing
    } else {
      (*foc.stream()) << input << endl;
      for (set<const TRule*>::iterator it = observer.used.begin(); it != observer.used.end(); ++it) {
        if (all_used.insert(*it).second)
          (*fog.stream()) << **it << endl;
      }
    }
  }
}

int main(int argc, char** argv) {
  register_feature_functions();

  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const string fname = conf["training_data"].as<string>();
  const unsigned ncpus = conf["ncpus"].as<unsigned>();
  const unsigned shards = conf["shards"].as<unsigned>();
  const unsigned start = conf["starting_shard"].as<unsigned>();
  const unsigned work_limit = conf["work_limit"].as<unsigned>();
  const unsigned eff_shards = min(start + work_limit, shards);
  cerr << "Processing shards " << start << "/" << shards << " to " << eff_shards << "/" << shards << endl;
  assert(ncpus > 0);
  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());
  if (decoder.GetConf()["input"].as<string>() != "-") {
    cerr << "cdec.ini must not set an input file\n";
    abort();
  }
  SetSilent(true);  // turn off verbose decoder output
  cerr << "Forking " << ncpus << " time(s)\n";
  vector<pid_t> children;
  for (int i = 0; i < ncpus; ++i) {
    pid_t pid = fork();
    if (pid < 0) {
      cerr << "Fork failed!\n";
      exit(1);
    }
    if (pid > 0) {
      children.push_back(pid);
    } else {
      for (int j = start; j < eff_shards; ++j) {
        if (j % ncpus == i) {
          cerr << "  CPU " << i << " processing shard " << j << endl;
          work(fname, j, shards, &decoder);
          cerr << "  Shard " << j << "/" << shards << " finished.\n";
        }
      }
      _exit(0);
    }
  }
  for (int i = 0; i < children.size(); ++i) {
    int status;
    int w = waitpid(children[i], &status, 0);
    if (w < 0) { cerr << "Error while waiting for children!"; return 1; }
    if (WIFSIGNALED(status)) {
      cerr << "Child " << i << " received signal " << WTERMSIG(status) << endl;
      if (WTERMSIG(status) == 11) { cerr << " this is a SEGV- you may be trying to print temporarily created rules\n"; }
    }
  }
  return 0;
}
