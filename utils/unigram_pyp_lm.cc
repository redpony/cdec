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
#include "gamma_poisson.h"

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

struct Histogram {
  void increment(unsigned bin, unsigned delta = 1u) {
    data[bin] += delta;
  }
  void decrement(unsigned bin, unsigned delta = 1u) {
    data[bin] -= delta;
  }
  void move(unsigned from_bin, unsigned to_bin, unsigned delta = 1u) {
    decrement(from_bin, delta);
    increment(to_bin, delta);
  }
  map<unsigned, unsigned> data;
  // SparseVector<unsigned> data;
};

// Lord Rothschild. 1986. THE DISTRIBUTION OF ENGLISH DICTIONARY WORD LENGTHS.
// Journal of Statistical Planning and Inference 14 (1986) 311-322
struct PoissonLengthUniformCharWordModel {
  explicit PoissonLengthUniformCharWordModel(unsigned vocab_size) : plen(5,5), uc(-log(50)), llh() {}
  void increment(WordID w, MT19937*) {
    llh += log(prob(w)); // this isn't quite right
    plen.increment(TD::Convert(w).size() - 1);
  }
  void decrement(WordID w, MT19937*) {
    plen.decrement(TD::Convert(w).size() - 1);
    llh -= log(prob(w)); // this isn't quite right
  }
  double prob(WordID w) const {
    size_t len = TD::Convert(w).size();
    return plen.prob(len - 1) * exp(uc * len);
  }
  double log_likelihood() const { return llh; }
  void resample_hyperparameters(MT19937*) {}
  GammaPoisson plen;
  const double uc;
  double llh;
};

// uniform base distribution (0-gram model)
struct UniformWordModel {
  explicit UniformWordModel(unsigned vocab_size) : p0(1.0 / vocab_size), draws() {}
  void increment(WordID, MT19937*) { ++draws; }
  void decrement(WordID, MT19937*) { --draws; assert(draws >= 0); }
  double prob(WordID) const { return p0; } // all words have equal prob
  double log_likelihood() const { return draws * log(p0); }
  void resample_hyperparameters(MT19937*) {}
  const double p0;
  int draws;
};

// represents an Unigram LM
template <class BaseGenerator>
struct UnigramLM {
  UnigramLM(unsigned vs, double da, double db, double ss, double sr) :
      base(vs),
      crp(da, db, ss, sr, 0.8, 1.0) {}
  void increment(WordID w, MT19937* rng) {
    const double backoff = base.prob(w);
    if (crp.increment(w, backoff, rng))
      base.increment(w, rng);
  }
  void decrement(WordID w, MT19937* rng) {
    if (crp.decrement(w, rng))
      base.decrement(w, rng);
  }
  double prob(WordID w) const {
    const double backoff = base.prob(w);
    return crp.prob(w, backoff);
  }

  double log_likelihood() const {
    double llh = base.log_likelihood();
    llh += crp.log_crp_prob();
    return llh;
  }

  void resample_hyperparameters(MT19937* rng) {
    crp.resample_hyperparameters(rng);
    base.resample_hyperparameters(rng);
  }

  double discount_a, discount_b, strength_s, strength_r;
  double d, strength;
  BaseGenerator base;
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
#if 1
  UnigramLM<PoissonLengthUniformCharWordModel> lm(vocabe.size(),
#else
  UnigramLM<UniformWordModel> lm(vocabe.size(),
#endif
                                 conf["discount_prior_a"].as<double>(),
                                 conf["discount_prior_b"].as<double>(),
                                 conf["strength_prior_s"].as<double>(),
                                 conf["strength_prior_r"].as<double>());
  for (unsigned SS=0; SS < samples; ++SS) {
    for (unsigned ci = 0; ci < corpuse.size(); ++ci) {
      const vector<WordID>& s = corpuse[ci];
      for (unsigned i = 0; i <= s.size(); ++i) {
        WordID w = (i < s.size() ? s[i] : kEOS);
        if (SS > 0) lm.decrement(w, &rng);
        lm.increment(w, &rng);
      }
      if (SS > 0) lm.decrement(kEOS, &rng);
      lm.increment(kEOS, &rng);
    }
    cerr << "LLH=" << lm.log_likelihood() << "\t tables=" << lm.crp.num_tables() << " " << endl;
    if (SS % 10 == 9) lm.resample_hyperparameters(&rng);
  }
  double llh = 0;
  unsigned cnt = 0;
  unsigned oovs = 0;
  for (unsigned ci = 0; ci < test.size(); ++ci) {
    const vector<WordID>& s = test[ci];
    for (unsigned i = 0; i <= s.size(); ++i) {
      WordID w = (i < s.size() ? s[i] : kEOS);
      double lp = log(lm.prob(w)) / log(2);
      if (i < s.size() && vocabe.count(w) == 0) {
        cerr << "**OOV ";
        ++oovs;
        //lp = 0;
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

