#include <iostream>
#include <tr1/memory>
#include <queue>

#include <boost/functional.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "viterbi.h"
#include "hg.h"
#include "trule.h"
#include "tdict.h"
#include "filelib.h"
#include "dict.h"
#include "sampler.h"
#include "ccrp_nt.h"
#include "ccrp_onetable.h"

using namespace std;
using namespace tr1;
namespace po = boost::program_options;

ostream& operator<<(ostream& os, const vector<WordID>& p) {
  os << '[';
  for (int i = 0; i < p.size(); ++i)
    os << (i==0 ? "" : " ") << TD::Convert(p[i]);
  return os << ']';
}

double log_poisson(unsigned x, const double& lambda) {
  assert(lambda > 0.0);
  return log(lambda) * x - lgamma(x + 1) - lambda;
}

struct Model1 {
  explicit Model1(const string& fname) :
      kNULL(TD::Convert("<eps>")),
      kZERO() {
    LoadModel1(fname);
  }

  void LoadModel1(const string& fname) {
    cerr << "Loading Model 1 parameters from " << fname << " ..." << endl;
    ReadFile rf(fname);
    istream& in = *rf.stream();
    string line;
    unsigned lc = 0;
    while(getline(in, line)) {
      ++lc;
      int cur = 0;
      int start = 0;
      while(cur < line.size() && line[cur] != ' ') { ++cur; }
      assert(cur != line.size());
      line[cur] = 0;
      const WordID src = TD::Convert(&line[0]);
      ++cur;
      start = cur;
      while(cur < line.size() && line[cur] != ' ') { ++cur; }
      assert(cur != line.size());
      line[cur] = 0;
      WordID trg = TD::Convert(&line[start]);
      const double logprob = strtod(&line[cur + 1], NULL);
      if (src >= ttable.size()) ttable.resize(src + 1);
      ttable[src][trg].logeq(logprob);
    }
    cerr << "  read " << lc << " parameters.\n";
  }

  // returns prob 0 if src or trg is not found!
  const prob_t& operator()(WordID src, WordID trg) const {
    if (src == 0) src = kNULL;
    if (src < ttable.size()) {
      const map<WordID, prob_t>& cpd = ttable[src];
      const map<WordID, prob_t>::const_iterator it = cpd.find(trg);
      if (it != cpd.end())
        return it->second;
    }
    return kZERO;
  }

  const WordID kNULL;
  const prob_t kZERO;
  vector<map<WordID, prob_t> > ttable;
};

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,s",po::value<unsigned>()->default_value(1000),"Number of samples")
        ("particles,p",po::value<unsigned>()->default_value(25),"Number of particles")
        ("input,i",po::value<string>(),"Read parallel data from")
        ("max_src_phrase",po::value<unsigned>()->default_value(7),"Maximum length of source language phrases")
        ("max_trg_phrase",po::value<unsigned>()->default_value(7),"Maximum length of target language phrases")
        ("model1,m",po::value<string>(),"Model 1 parameters (used in base distribution)")
        ("inverse_model1,M",po::value<string>(),"Inverse Model 1 parameters (used in backward estimate)")
        ("model1_interpolation_weight",po::value<double>()->default_value(0.95),"Mixing proportion of model 1 with uniform target distribution")
        ("random_seed,S",po::value<uint32_t>(), "Random seed");
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

  if (conf->count("help") || (conf->count("input") == 0)) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

void ReadParallelCorpus(const string& filename,
                vector<vector<WordID> >* f,
                vector<vector<WordID> >* e,
                set<WordID>* vocab_f,
                set<WordID>* vocab_e) {
  f->clear();
  e->clear();
  vocab_f->clear();
  vocab_e->clear();
  istream* in;
  if (filename == "-")
    in = &cin;
  else
    in = new ifstream(filename.c_str());
  assert(*in);
  string line;
  const WordID kDIV = TD::Convert("|||");
  vector<WordID> tmp;
  while(*in) {
    getline(*in, line);
    if (line.empty() && !*in) break;
    e->push_back(vector<int>());
    f->push_back(vector<int>());
    vector<int>& le = e->back();
    vector<int>& lf = f->back();
    tmp.clear();
    TD::ConvertSentence(line, &tmp);
    bool isf = true;
    for (unsigned i = 0; i < tmp.size(); ++i) {
      const int cur = tmp[i];
      if (isf) {
        if (kDIV == cur) { isf = false; } else {
          lf.push_back(cur);
          vocab_f->insert(cur);
        }
      } else {
        assert(cur != kDIV);
        le.push_back(cur);
        vocab_e->insert(cur);
      }
    }
    assert(isf == false);
  }
  if (in != &cin) delete in;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const size_t kMAX_TRG_PHRASE = conf["max_trg_phrase"].as<unsigned>();
  const size_t kMAX_SRC_PHRASE = conf["max_src_phrase"].as<unsigned>();
  const unsigned particles = conf["particles"].as<unsigned>();
  const unsigned samples = conf["samples"].as<unsigned>();

  if (!conf.count("model1")) {
    cerr << argv[0] << "Please use --model1 to specify model 1 parameters\n";
    return 1;
  }
  shared_ptr<MT19937> prng;
  if (conf.count("random_seed"))
    prng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    prng.reset(new MT19937);
  MT19937& rng = *prng;

  vector<vector<WordID> > corpuse, corpusf;
  set<WordID> vocabe, vocabf;
  cerr << "Reading corpus...\n";
  ReadParallelCorpus(conf["input"].as<string>(), &corpusf, &corpuse, &vocabf, &vocabe);
  cerr << "F-corpus size: " << corpusf.size() << " sentences\t (" << vocabf.size() << " word types)\n";
  cerr << "E-corpus size: " << corpuse.size() << " sentences\t (" << vocabe.size() << " word types)\n";
  assert(corpusf.size() == corpuse.size());

  const int kLHS = -TD::Convert("X");
  Model1 m1(conf["model1"].as<string>());
  Model1 invm1(conf["inverse_model1"].as<string>());
  for (int si = 0; si < conf["samples"].as<unsigned>(); ++si) {
    cerr << '.' << flush;
    for (int ci = 0; ci < corpusf.size(); ++ci) {
      const vector<WordID>& src = corpusf[ci];
      const vector<WordID>& trg = corpuse[ci];
      for (int i = 0; i < src.size(); ++i) {
        for (int j = 0; j < trg.size(); ++j) {
          const int eff_max_src = min(src.size() - i, kMAX_SRC_PHRASE);
          for (int k = 0; k < eff_max_src; ++k) {
            const int eff_max_trg = (k == 0 ? 1 : min(trg.size() - j, kMAX_TRG_PHRASE));
            for (int l = 0; l < eff_max_trg; ++l) {
            }
          }
        }
      }
    }
  }
}

