#include <iostream>
#include <tr1/memory>
#include <queue>

#include <boost/functional.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "corpus_tools.h"
#include "m.h"
#include "tdict.h"
#include "sampler.h"
#include "ccrp.h"

// A not very memory-efficient implementation of an 1-gram LM based on PYPs
// as described in Y.-W. Teh. (2006) A Hierarchical Bayesian Language Model
// based on Pitman-Yor Processes. In Proc. ACL.

using namespace std;
using namespace tr1;
namespace po = boost::program_options;

boost::shared_ptr<MT19937> prng;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,n",po::value<unsigned>()->default_value(50),"Number of samples")
        ("train,i",po::value<string>(),"Training data file")
        ("test,T",po::value<string>(),"Test data file")
        ("discount_prior_a,a",po::value<double>()->default_value(1.0), "discount ~ Beta(a,b): a=this")
        ("discount_prior_b,b",po::value<double>()->default_value(1.0), "discount ~ Beta(a,b): b=this")
        ("strength_prior_s,s",po::value<double>()->default_value(1.0), "strength ~ Gamma(s,r): s=this")
        ("strength_prior_r,r",po::value<double>()->default_value(1.0), "strength ~ Gamma(s,r): r=this")
        ("random_seed,S",po::value<uint32_t>(), "Random seed");
  po::options_description clo("Command line options");
  clo.add_options()
        ("config", po::value<string>(), "Configuration file")
        ("help", "Print this help message and exit");
  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts);
  dcmdline_options.add(opts).add(clo);
  
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("config")) {
    ifstream config((*conf)["config"].as<string>().c_str());
    po::store(po::parse_config_file(config, dconfig_options), *conf);
  }
  po::notify(*conf);

  if (conf->count("help") || (conf->count("train") == 0)) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

// uniform base distribution (0-gram model)
struct UniformWordModel {
  explicit UniformWordModel(unsigned vocab_size) : p0(1.0 / vocab_size), draws() {}
  void increment() { ++draws; }
  void decrement() { --draws; assert(draws >= 0); }
  double prob(WordID) const { return p0; } // all words have equal prob
  double log_likelihood() const { return draws * log(p0); }
  const double p0;
  int draws;
};

// represents an Unigram LM
struct UnigramLM {
  UnigramLM(unsigned vs, double da, double db, double ss, double sr) :
      uniform_vocab(vs),
      crp(da, db, ss, sr, 0.8, 1.0) {}
  void increment(WordID w, MT19937* rng) {
    const double backoff = uniform_vocab.prob(w);
    if (crp.increment(w, backoff, rng))
      uniform_vocab.increment();
  }
  void decrement(WordID w, MT19937* rng) {
    if (crp.decrement(w, rng))
      uniform_vocab.decrement();
  }
  double prob(WordID w) const {
    const double backoff = uniform_vocab.prob(w);
    return crp.prob(w, backoff);
  }

  double log_likelihood() const {
    double llh = uniform_vocab.log_likelihood();
    llh += crp.log_crp_prob();
    return llh;
  }

  void resample_hyperparameters(MT19937* rng) {
    crp.resample_hyperparameters(rng);
  }

  double discount_a, discount_b, strength_s, strength_r;
  double d, strength;
  UniformWordModel uniform_vocab;
  CCRP<WordID> crp;
};

int main(int argc, char** argv) {
  po::variables_map conf;

  InitCommandLine(argc, argv, &conf);
  const unsigned samples = conf["samples"].as<unsigned>();
  if (conf.count("random_seed"))
    prng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    prng.reset(new MT19937);
  MT19937& rng = *prng;
  vector<vector<WordID> > corpuse;
  set<WordID> vocabe;
  const WordID kEOS = TD::Convert("</s>");
  cerr << "Reading corpus...\n";
  CorpusTools::ReadFromFile(conf["train"].as<string>(), &corpuse, &vocabe);
  cerr << "E-corpus size: " << corpuse.size() << " sentences\t (" << vocabe.size() << " word types)\n";
  vector<vector<WordID> > test;
  if (conf.count("test"))
    CorpusTools::ReadFromFile(conf["test"].as<string>(), &test);
  else
    test = corpuse;
  UnigramLM lm(vocabe.size(),
               conf["discount_prior_a"].as<double>(),
               conf["discount_prior_b"].as<double>(),
               conf["strength_prior_s"].as<double>(),
               conf["strength_prior_r"].as<double>());
  for (int SS=0; SS < samples; ++SS) {
    for (int ci = 0; ci < corpuse.size(); ++ci) {
      const vector<WordID>& s = corpuse[ci];
      for (int i = 0; i <= s.size(); ++i) {
        WordID w = (i < s.size() ? s[i] : kEOS);
        if (SS > 0) lm.decrement(w, &rng);
        lm.increment(w, &rng);
      }
      if (SS > 0) lm.decrement(kEOS, &rng);
      lm.increment(kEOS, &rng);
    }
    cerr << "LLH=" << lm.log_likelihood() << endl;
    //if (SS % 10 == 9) lm.resample_hyperparameters(&rng);
  }
  double llh = 0;
  unsigned cnt = 0;
  unsigned oovs = 0;
  for (int ci = 0; ci < test.size(); ++ci) {
    const vector<WordID>& s = test[ci];
    for (int i = 0; i <= s.size(); ++i) {
      WordID w = (i < s.size() ? s[i] : kEOS);
      double lp = log(lm.prob(w)) / log(2);
      if (i < s.size() && vocabe.count(w) == 0) {
        cerr << "**OOV ";
        ++oovs;
        lp = 0;
      }
      cerr << "p(" << TD::Convert(w) << ") = " << lp << endl;
      llh -= lp;
      cnt++;
    }
  }
  cerr << "  Log_10 prob: " << (-llh * log(2) / log(10)) << endl;
  cerr << "        Count: " << cnt << endl;
  cerr << "         OOVs: " << oovs << endl;
  cerr << "Cross-entropy: " << (llh / cnt) << endl;
  cerr << "   Perplexity: " << pow(2, llh / cnt) << endl;
  return 0;
}

