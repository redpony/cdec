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
#include "tied_resampler.h"

// A not very memory-efficient implementation of an N-gram LM based on PYPs
// as described in Y.-W. Teh. (2006) A Hierarchical Bayesian Language Model
// based on Pitman-Yor Processes. In Proc. ACL.

// I use templates to handle the recursive formalation of the prior, so
// the order of the model has to be specified here, at compile time:
#define kORDER 3

using namespace std;
using namespace tr1;
namespace po = boost::program_options;

boost::shared_ptr<MT19937> prng;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,n",po::value<unsigned>()->default_value(300),"Number of samples")
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

template <unsigned N> struct PYPLM;

// uniform base distribution (0-gram model)
template<> struct PYPLM<0> {
  PYPLM(unsigned vs, double, double, double, double) : p0(1.0 / vs), draws() {}
  void increment(WordID, const vector<WordID>&, MT19937*) { ++draws; }
  void decrement(WordID, const vector<WordID>&, MT19937*) { --draws; assert(draws >= 0); }
  double prob(WordID, const vector<WordID>&) const { return p0; }
  void resample_hyperparameters(MT19937*) {}
  double log_likelihood() const { return draws * log(p0); }
  const double p0;
  int draws;
};

// represents an N-gram LM
template <unsigned N> struct PYPLM {
  PYPLM(unsigned vs, double da, double db, double ss, double sr) :
      backoff(vs, da, db, ss, sr),
      tr(da, db, ss, sr, 0.8, 1.0),
      lookup(N-1) {}
  void increment(WordID w, const vector<WordID>& context, MT19937* rng) {
    const double bo = backoff.prob(w, context);
    for (unsigned i = 0; i < N-1; ++i)
      lookup[i] = context[context.size() - 1 - i];
    typename unordered_map<vector<WordID>, CCRP<WordID>, boost::hash<vector<WordID> > >::iterator it = p.find(lookup);
    if (it == p.end()) {
      it = p.insert(make_pair(lookup, CCRP<WordID>(0.5,1))).first;
      tr.Add(&it->second);  // add to resampler
    }
    if (it->second.increment(w, bo, rng))
      backoff.increment(w, context, rng);
  }
  void decrement(WordID w, const vector<WordID>& context, MT19937* rng) {
    for (unsigned i = 0; i < N-1; ++i)
      lookup[i] = context[context.size() - 1 - i];
    typename unordered_map<vector<WordID>, CCRP<WordID>, boost::hash<vector<WordID> > >::iterator it = p.find(lookup);
    assert(it != p.end());
    if (it->second.decrement(w, rng))
      backoff.decrement(w, context, rng);
  }
  double prob(WordID w, const vector<WordID>& context) const {
    const double bo = backoff.prob(w, context);
    for (unsigned i = 0; i < N-1; ++i)
      lookup[i] = context[context.size() - 1 - i];
    typename unordered_map<vector<WordID>, CCRP<WordID>, boost::hash<vector<WordID> > >::const_iterator it = p.find(lookup);
    if (it == p.end()) return bo;
    return it->second.prob(w, bo);
  }

  double log_likelihood() const {
    double llh = backoff.log_likelihood();
    typename unordered_map<vector<WordID>, CCRP<WordID>, boost::hash<vector<WordID> > >::const_iterator it;
    for (it = p.begin(); it != p.end(); ++it)
      llh += it->second.log_crp_prob();
    llh += tr.LogLikelihood();
    return llh;
  }

  void resample_hyperparameters(MT19937* rng) {
    tr.ResampleHyperparameters(rng);
    backoff.resample_hyperparameters(rng);
  }

  PYPLM<N-1> backoff;
  TiedResampler<CCRP<WordID> > tr;
  double discount_a, discount_b, strength_s, strength_r;
  double d, strength;
  mutable vector<WordID> lookup;  // thread-local
  unordered_map<vector<WordID>, CCRP<WordID>, boost::hash<vector<WordID> > > p;
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
  PYPLM<kORDER> lm(vocabe.size(),
                   conf["discount_prior_a"].as<double>(),
                   conf["discount_prior_b"].as<double>(),
                   conf["strength_prior_s"].as<double>(),
                   conf["strength_prior_r"].as<double>());
  vector<WordID> ctx(kORDER - 1, TD::Convert("<s>"));
  for (int SS=0; SS < samples; ++SS) {
    for (int ci = 0; ci < corpuse.size(); ++ci) {
      ctx.resize(kORDER - 1);
      const vector<WordID>& s = corpuse[ci];
      for (int i = 0; i <= s.size(); ++i) {
        WordID w = (i < s.size() ? s[i] : kEOS);
        if (SS > 0) lm.decrement(w, ctx, &rng);
        lm.increment(w, ctx, &rng);
        ctx.push_back(w);
      }
      if (SS > 0) lm.decrement(kEOS, ctx, &rng);
      lm.increment(kEOS, ctx, &rng);
    }
    if (SS % 10 == 9) {
      cerr << " [LLH=" << lm.log_likelihood() << "]" << endl;
      if (SS % 20 == 19) lm.resample_hyperparameters(&rng);
    } else { cerr << '.' << flush; }
  }
  double llh = 0;
  unsigned cnt = 0;
  unsigned oovs = 0;
  for (int ci = 0; ci < test.size(); ++ci) {
    ctx.resize(kORDER - 1);
    const vector<WordID>& s = test[ci];
    for (int i = 0; i <= s.size(); ++i) {
      WordID w = (i < s.size() ? s[i] : kEOS);
      double lp = log(lm.prob(w, ctx)) / log(2);
      if (i < s.size() && vocabe.count(w) == 0) {
        cerr << "**OOV ";
        ++oovs;
        lp = 0;
      }
      cerr << "p(" << TD::Convert(w) << " |";
      for (int j = ctx.size() + 1 - kORDER; j < ctx.size(); ++j)
        cerr << ' ' << TD::Convert(ctx[j]);
      cerr << ") = " << lp << endl;
      ctx.push_back(w);
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


