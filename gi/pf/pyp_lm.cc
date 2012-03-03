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
#include "ccrp_onetable.h"

using namespace std;
using namespace tr1;
namespace po = boost::program_options;

shared_ptr<MT19937> prng;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,s",po::value<unsigned>()->default_value(1000),"Number of samples")
        ("input,i",po::value<string>(),"Read data from")
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

  if (conf->count("help") || (conf->count("input") == 0)) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

template <unsigned N> struct PYPLM;

// uniform base distribution
template<> struct PYPLM<0> {
  PYPLM(unsigned vs) : p0(1.0 / vs), draws() {}
  void increment(WordID w, const vector<WordID>& context, MT19937* rng) { ++draws; }
  void decrement(WordID w, const vector<WordID>& context, MT19937* rng) { --draws; assert(draws >= 0); }
  double prob(WordID w, const vector<WordID>& context) const { return p0; }
  void resample_hyperparameters(MT19937* rng, const unsigned nloop, const unsigned niterations) {}
  double log_likelihood() const { return draws * log(p0); }
  const double p0;
  int draws;
};

// represents an N-gram LM
template <unsigned N> struct PYPLM {
  PYPLM(unsigned vs) : backoff(vs), d(0.8), alpha(1.0) {}
  void increment(WordID w, const vector<WordID>& context, MT19937* rng) {
    const double bo = backoff.prob(w, context);
    static vector<WordID> lookup(N-1);
    for (unsigned i = 0; i < N-1; ++i)
      lookup[i] = context[context.size() - 1 - i];
    typename unordered_map<vector<WordID>, CCRP<WordID>, boost::hash<vector<WordID> > >::iterator it = p.find(lookup);
    if (it == p.end())
      it = p.insert(make_pair(lookup, CCRP<WordID>(d,alpha))).first;
    if (it->second.increment(w, bo, rng))
      backoff.increment(w, context, rng);
  }
  void decrement(WordID w, const vector<WordID>& context, MT19937* rng) {
    static vector<WordID> lookup(N-1);
    for (unsigned i = 0; i < N-1; ++i)
      lookup[i] = context[context.size() - 1 - i];
    typename unordered_map<vector<WordID>, CCRP<WordID>, boost::hash<vector<WordID> > >::iterator it = p.find(lookup);
    assert(it != p.end());
    if (it->second.decrement(w, rng))
      backoff.decrement(w, context, rng);
  }
  double prob(WordID w, const vector<WordID>& context) const {
    const double bo = backoff.prob(w, context);
    static vector<WordID> lookup(N-1);
    for (unsigned i = 0; i < N-1; ++i)
      lookup[i] = context[context.size() - 1 - i];
    typename unordered_map<vector<WordID>, CCRP<WordID>, boost::hash<vector<WordID> > >::const_iterator it = p.find(lookup);
    if (it == p.end()) return bo;
    return it->second.prob(w, bo);
  }

  double log_likelihood() const {
    return log_likelihood(d, alpha) + backoff.log_likelihood();
  }

  double log_likelihood(const double& dd, const double& aa) const {
    if (aa <= -dd) return -std::numeric_limits<double>::infinity();
    double llh = Md::log_beta_density(dd, 1, 1) + Md::log_gamma_density(aa, 1, 1);
    typename unordered_map<vector<WordID>, CCRP<WordID>, boost::hash<vector<WordID> > >::const_iterator it;
    for (it = p.begin(); it != p.end(); ++it)
      llh += it->second.log_crp_prob(dd, aa);
    return llh;
  }

  struct DiscountResampler {
    DiscountResampler(const PYPLM& m) : m_(m) {}
    const PYPLM& m_;
    double operator()(const double& proposed_discount) const {
      return m_.log_likelihood(proposed_discount, m_.alpha);
    }
  };

  struct AlphaResampler {
    AlphaResampler(const PYPLM& m) : m_(m) {}
    const PYPLM& m_;
    double operator()(const double& proposed_alpha) const {
      return m_.log_likelihood(m_.d, proposed_alpha);
    }
  };

  void resample_hyperparameters(MT19937* rng, const unsigned nloop = 5, const unsigned niterations = 10) {
    DiscountResampler dr(*this);
    AlphaResampler ar(*this);
    for (int iter = 0; iter < nloop; ++iter) {
      alpha = slice_sampler1d(ar, alpha, *rng, 0.0,
                              std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
      d = slice_sampler1d(dr, d, *rng, std::numeric_limits<double>::min(),
                          1.0, 0.0, niterations, 100*niterations);
    }
    alpha = slice_sampler1d(ar, alpha, *rng, 0.0,
                            std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
    typename unordered_map<vector<WordID>, CCRP<WordID>, boost::hash<vector<WordID> > >::iterator it;
    cerr << "PYPLM<" << N << ">(d=" << d << ",a=" << alpha << ") = " << log_likelihood(d, alpha) << endl;
    for (it = p.begin(); it != p.end(); ++it) {
      it->second.set_discount(d);
      it->second.set_alpha(alpha);
    }
    backoff.resample_hyperparameters(rng, nloop, niterations);
  }

  PYPLM<N-1> backoff;
  double d, alpha;
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
  CorpusTools::ReadFromFile(conf["input"].as<string>(), &corpuse, &vocabe);
  cerr << "E-corpus size: " << corpuse.size() << " sentences\t (" << vocabe.size() << " word types)\n";
#define kORDER 3
  PYPLM<kORDER> lm(vocabe.size());
  vector<WordID> ctx(kORDER - 1, TD::Convert("<s>"));
  int mci = corpuse.size() * 99 / 100;
  for (int SS=0; SS < samples; ++SS) {
    for (int ci = 0; ci < mci; ++ci) {
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
  for (int ci = mci; ci < corpuse.size(); ++ci) {
    ctx.resize(kORDER - 1);
    const vector<WordID>& s = corpuse[ci];
    for (int i = 0; i <= s.size(); ++i) {
      WordID w = (i < s.size() ? s[i] : kEOS);
      double lp = log(lm.prob(w, ctx)) / log(2);
      cerr << "p(" << TD::Convert(w) << " | " << TD::GetString(ctx) << ") = " << lp << endl;
      ctx.push_back(w);
      llh -= lp;
      cnt++;
    }
  }
  cerr << "  Log_10 prob: " << (llh * log(2) / log(10)) << endl;
  cerr << "        Count: " << (cnt) << endl;
  cerr << "Cross-entropy: " << (llh / cnt) << endl;
  cerr << "   Perplexity: " << pow(2, llh / cnt) << endl;
  return 0;
}

