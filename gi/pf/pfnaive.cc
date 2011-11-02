#include <iostream>
#include <tr1/memory>
#include <queue>

#include <boost/functional.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "pf.h"
#include "base_measures.h"
#include "monotonic_pseg.h"
#include "reachability.h"
#include "viterbi.h"
#include "hg.h"
#include "trule.h"
#include "tdict.h"
#include "filelib.h"
#include "dict.h"
#include "sampler.h"
#include "ccrp_nt.h"
#include "ccrp_onetable.h"
#include "corpus.h"

using namespace std;
using namespace tr1;
namespace po = boost::program_options;

shared_ptr<MT19937> prng;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,s",po::value<unsigned>()->default_value(1000),"Number of samples")
        ("particles,p",po::value<unsigned>()->default_value(30),"Number of particles")
        ("filter_frequency,f",po::value<unsigned>()->default_value(5),"Number of time steps between filterings")
        ("input,i",po::value<string>(),"Read parallel data from")
        ("max_src_phrase",po::value<unsigned>()->default_value(5),"Maximum length of source language phrases")
        ("max_trg_phrase",po::value<unsigned>()->default_value(5),"Maximum length of target language phrases")
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

struct BackwardEstimateSym {
  BackwardEstimateSym(const Model1& m1,
                      const Model1& invm1, const vector<WordID>& src, const vector<WordID>& trg) :
      model1_(m1), invmodel1_(invm1), src_(src), trg_(trg) {
  }
  const prob_t& operator()(unsigned src_cov, unsigned trg_cov) const {
    assert(src_cov <= src_.size());
    assert(trg_cov <= trg_.size());
    prob_t& e = cache_[src_cov][trg_cov];
    if (e.is_0()) {
      if (trg_cov == trg_.size()) { e = prob_t::One(); return e; }
      vector<WordID> r(src_.size() + 1); r.clear();
      for (int i = src_cov; i < src_.size(); ++i)
        r.push_back(src_[i]);
      r.push_back(0);  // NULL word
      const prob_t uniform_alignment(1.0 / r.size());
      e.logeq(log_poisson(trg_.size() - trg_cov, r.size() - 1)); // p(trg len remaining | src len remaining)
      for (unsigned j = trg_cov; j < trg_.size(); ++j) {
        prob_t p;
        for (unsigned i = 0; i < r.size(); ++i)
          p += model1_(r[i], trg_[j]);
        if (p.is_0()) {
          cerr << "ERROR: p(" << TD::Convert(trg_[j]) << " | " << TD::GetString(r) << ") = 0!\n";
          abort();
        }
        p *= uniform_alignment;
        e *= p;
      }
      r.pop_back();
      const prob_t inv_uniform(1.0 / (trg_.size() - trg_cov + 1.0));
      prob_t inv;
      inv.logeq(log_poisson(r.size(), trg_.size() - trg_cov));
      for (unsigned i = 0; i < r.size(); ++i) {
        prob_t p;
        for (unsigned j = trg_cov - 1; j < trg_.size(); ++j)
          p += invmodel1_(j < trg_cov ? 0 : trg_[j], r[i]);
        if (p.is_0()) {
          cerr << "ERROR: p_inv(" << TD::Convert(r[i]) << " | " << TD::GetString(trg_) << ") = 0!\n";
          abort();
        }
        p *= inv_uniform;
        inv *= p;
      }
      prob_t x = pow(e * inv, 0.5);
      e = x;
      //cerr << "Forward: " << log(e) << "\tBackward: " << log(inv) << "\t prop: " << log(x) << endl;
    }
    return e;
  }
  const Model1& model1_;
  const Model1& invmodel1_;
  const vector<WordID>& src_;
  const vector<WordID>& trg_;
  mutable unordered_map<unsigned, map<unsigned, prob_t> > cache_;
};

struct Particle {
  Particle() : weight(prob_t::One()), src_cov(), trg_cov() {}
  prob_t weight;
  prob_t gamma_last;
  vector<TRulePtr> rules;
  int src_cov;
  int trg_cov;
};

ostream& operator<<(ostream& o, const vector<bool>& v) {
  for (int i = 0; i < v.size(); ++i)
    o << (v[i] ? '1' : '0');
  return o;
}
ostream& operator<<(ostream& o, const Particle& p) {
  o << "[src_cov=" << p.src_cov << " trg_cov=" << p.trg_cov << " num_rules=" << p.rules.size() << "  w=" << log(p.weight) << ']';
  return o;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const unsigned kMAX_TRG_PHRASE = conf["max_trg_phrase"].as<unsigned>();
  const unsigned kMAX_SRC_PHRASE = conf["max_src_phrase"].as<unsigned>();
  const unsigned particles = conf["particles"].as<unsigned>();
  const unsigned samples = conf["samples"].as<unsigned>();
  const unsigned rejuv_freq = conf["filter_frequency"].as<unsigned>();

  if (!conf.count("model1")) {
    cerr << argv[0] << "Please use --model1 to specify model 1 parameters\n";
    return 1;
  }
  if (conf.count("random_seed"))
    prng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    prng.reset(new MT19937);
  MT19937& rng = *prng;

  vector<vector<WordID> > corpuse, corpusf;
  set<WordID> vocabe, vocabf;
  cerr << "Reading corpus...\n";
  corpus::ReadParallelCorpus(conf["input"].as<string>(), &corpusf, &corpuse, &vocabf, &vocabe);
  cerr << "F-corpus size: " << corpusf.size() << " sentences\t (" << vocabf.size() << " word types)\n";
  cerr << "E-corpus size: " << corpuse.size() << " sentences\t (" << vocabe.size() << " word types)\n";
  assert(corpusf.size() == corpuse.size());

  const int kLHS = -TD::Convert("X");
  Model1 m1(conf["model1"].as<string>());
  Model1 invm1(conf["inverse_model1"].as<string>());

  PhraseJointBase lp0(m1, conf["model1_interpolation_weight"].as<double>(), vocabe.size(), vocabf.size());
  PhraseJointBase_BiDir alp0(m1, invm1, conf["model1_interpolation_weight"].as<double>(), vocabe.size(), vocabf.size());
  MonotonicParallelSegementationModel<PhraseJointBase_BiDir> m(alp0);
  TRule xx("[X] ||| ms. kimura ||| MS. KIMURA ||| X=0");
  cerr << xx << endl << lp0(xx) << " " << alp0(xx) << endl;
  TRule xx12("[X] ||| . ||| PHARMACY . ||| X=0");
  TRule xx21("[X] ||| pharmacy . ||| . ||| X=0");
//  TRule xx22("[X] ||| . ||| . ||| X=0");
  TRule xx22("[X] ||| . ||| THE . ||| X=0");
  cerr << xx12 << "\t" << lp0(xx12) << " " << alp0(xx12) << endl;
  cerr << xx21 << "\t" << lp0(xx21) << " " << alp0(xx21) << endl;
  cerr << xx22 << "\t" << lp0(xx22) << " " << alp0(xx22) << endl;

  cerr << "Initializing reachability limits...\n";
  vector<Particle> ps(corpusf.size());
  vector<Reachability> reaches; reaches.reserve(corpusf.size());
  for (int ci = 0; ci < corpusf.size(); ++ci)
    reaches.push_back(Reachability(corpusf[ci].size(),
                                   corpuse[ci].size(),
                                   kMAX_SRC_PHRASE,
                                   kMAX_TRG_PHRASE));
  cerr << "Sampling...\n"; 
  vector<Particle> tmp_p(10000);  // work space
  SampleSet<prob_t> pfss;
  SystematicResampleFilter<Particle> filter(&rng);
  // MultinomialResampleFilter<Particle> filter(&rng);
  for (int SS=0; SS < samples; ++SS) {
    for (int ci = 0; ci < corpusf.size(); ++ci) {
      vector<int>& src = corpusf[ci];
      vector<int>& trg = corpuse[ci];
      m.DecrementRulesAndStops(ps[ci].rules);
      const prob_t q_stop = m.StopProbability();
      const prob_t q_cont = m.ContinueProbability();
      cerr << "P(stop)=" << q_stop << "\tP(continue)=" <<q_cont << endl;

      BackwardEstimateSym be(m1, invm1, src, trg);
      const Reachability& r = reaches[ci];
      vector<Particle> lps(particles);

      bool all_complete = false;
      while(!all_complete) {
        SampleSet<prob_t> ss;

        // all particles have now been extended a bit, we will reweight them now
        if (lps[0].trg_cov > 0)
          filter(&lps);

        // loop over all particles and extend them
        bool done_nothing = true;
        for (int pi = 0; pi < particles; ++pi) {
          Particle& p = lps[pi];
          int tic = 0;
          while(p.trg_cov < trg.size() && tic < rejuv_freq) {
            ++tic;
            done_nothing = false;
            ss.clear();
            TRule x; x.lhs_ = kLHS;
            prob_t z;

            for (int trg_len = 1; trg_len <= kMAX_TRG_PHRASE; ++trg_len) {
              x.e_.push_back(trg[trg_len - 1 + p.trg_cov]);
              for (int src_len = 1; src_len <= kMAX_SRC_PHRASE; ++src_len) {
                if (!r.edges[p.src_cov][p.trg_cov][src_len][trg_len]) continue;

                int i = p.src_cov;
                assert(ss.size() < tmp_p.size());  // if fails increase tmp_p size
                Particle& np = tmp_p[ss.size()];
                np = p;
                x.f_.clear();
                for (int j = 0; j < src_len; ++j)
                  x.f_.push_back(src[i + j]);
                np.src_cov += x.f_.size();
                np.trg_cov += x.e_.size();
                const bool stop_now = (np.src_cov == src_len && np.trg_cov == trg_len);
                prob_t rp = m.RuleProbability(x) * (stop_now ? q_stop : q_cont);
                np.gamma_last = rp;
                const prob_t u = pow(np.gamma_last * pow(be(np.src_cov, np.trg_cov), 1.2), 0.1);
                //cerr << "**rule=" << x << endl;
                //cerr << "  u=" << log(u) << "  rule=" << rp << endl;
                ss.add(u);
                np.rules.push_back(TRulePtr(new TRule(x)));
                z += u;
              }
            }
            //cerr << "number of edges to consider: " << ss.size() << endl;
            const int sampled = rng.SelectSample(ss);
            prob_t q_n = ss[sampled] / z;
            p = tmp_p[sampled];
            //m.IncrementRule(*p.rules.back());
            p.weight *= p.gamma_last / q_n;
            //cerr << "[w=" << log(p.weight) << "]\tsampled rule: " << p.rules.back()->AsString() << endl;
            //cerr << p << endl;
          }
        } // loop over particles (pi = 0 .. particles)
        if (done_nothing) all_complete = true;
        prob_t wv = prob_t::Zero();
        for (int pp = 0; pp < lps.size(); ++pp)
          wv += lps[pp].weight;
        for (int pp = 0; pp < lps.size(); ++pp)
          lps[pp].weight /= wv;
      }
      pfss.clear();
      for (int i = 0; i < lps.size(); ++i)
        pfss.add(lps[i].weight);
      const int sampled = rng.SelectSample(pfss);
      ps[ci] = lps[sampled];
      m.IncrementRulesAndStops(lps[sampled].rules);
      for (int i = 0; i < lps[sampled].rules.size(); ++i) { cerr << "S:\t" << lps[sampled].rules[i]->AsString() << "\n"; }
      cerr << "tmp-LLH: " << log(m.Likelihood()) << endl;
    }
    cerr << "LLH: " << log(m.Likelihood()) << endl;
  }
  return 0;
}

