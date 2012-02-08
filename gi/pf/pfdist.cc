#include <iostream>
#include <tr1/memory>
#include <queue>

#include <boost/functional.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "pf.h"
#include "base_distributions.h"
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

#if 0
struct MyConditionalModel {
  MyConditionalModel(PhraseConditionalBase& rcp0) : rp0(&rcp0), base(prob_t::One()), src_phrases(1,1), src_jumps(200, CCRP_NoTable<int>(1,1)) {}

  prob_t srcp0(const vector<WordID>& src) const {
    prob_t p(1.0 / 3000.0);
    p.poweq(src.size());
    prob_t lenp; lenp.logeq(log_poisson(src.size(), 1.0));
    p *= lenp;
    return p;
  }

  void DecrementRule(const TRule& rule) {
    const RuleCRPMap::iterator it = rules.find(rule.f_);
    assert(it != rules.end());
    if (it->second.decrement(rule)) {
      base /= (*rp0)(rule);
      if (it->second.num_customers() == 0)
        rules.erase(it);
    }
    if (src_phrases.decrement(rule.f_))
      base /= srcp0(rule.f_);
  }

  void IncrementRule(const TRule& rule) {
    RuleCRPMap::iterator it = rules.find(rule.f_);
    if (it == rules.end())
      it = rules.insert(make_pair(rule.f_, CCRP_NoTable<TRule>(1,1))).first;
    if (it->second.increment(rule)) {
      base *= (*rp0)(rule);
    }
    if (src_phrases.increment(rule.f_))
      base *= srcp0(rule.f_);
  }

  void IncrementRules(const vector<TRulePtr>& rules) {
    for (int i = 0; i < rules.size(); ++i)
      IncrementRule(*rules[i]);
  }

  void DecrementRules(const vector<TRulePtr>& rules) {
    for (int i = 0; i < rules.size(); ++i)
      DecrementRule(*rules[i]);
  }

  void IncrementJump(int dist, unsigned src_len) {
    assert(src_len > 0);
    if (src_jumps[src_len].increment(dist))
      base *= jp0(dist, src_len);
  }

  void DecrementJump(int dist, unsigned src_len) {
    assert(src_len > 0);
    if (src_jumps[src_len].decrement(dist))
      base /= jp0(dist, src_len);
  }

  void IncrementJumps(const vector<int>& js, unsigned src_len) {
    for (unsigned i = 0; i < js.size(); ++i)
      IncrementJump(js[i], src_len);
  }

  void DecrementJumps(const vector<int>& js, unsigned src_len) {
    for (unsigned i = 0; i < js.size(); ++i)
      DecrementJump(js[i], src_len);
  }

  // p(jump = dist | src_len , z)
  prob_t JumpProbability(int dist, unsigned src_len) {
    const prob_t p0 = jp0(dist, src_len);
    const double lp = src_jumps[src_len].logprob(dist, log(p0));
    prob_t q; q.logeq(lp);
    return q;
  }

  // p(rule.f_ | z) * p(rule.e_ | rule.f_ , z)
  prob_t RuleProbability(const TRule& rule) const {
    const prob_t p0 = (*rp0)(rule);
    prob_t srcp; srcp.logeq(src_phrases.logprob(rule.f_, log(srcp0(rule.f_))));
    const RuleCRPMap::const_iterator it = rules.find(rule.f_);
    if (it == rules.end()) return srcp * p0;
    const double lp = it->second.logprob(rule, log(p0));
    prob_t q; q.logeq(lp);
    return q * srcp;
  }

  prob_t Likelihood() const {
    prob_t p = base;
    for (RuleCRPMap::const_iterator it = rules.begin();
         it != rules.end(); ++it) {
      prob_t cl; cl.logeq(it->second.log_crp_prob());
      p *= cl;
    }
    for (unsigned l = 1; l < src_jumps.size(); ++l) {
      if (src_jumps[l].num_customers() > 0) {
        prob_t q;
        q.logeq(src_jumps[l].log_crp_prob());
        p *= q;
      }
    }
    return p;
  }

  JumpBase jp0;
  const PhraseConditionalBase* rp0;
  prob_t base;
  typedef unordered_map<vector<WordID>, CCRP_NoTable<TRule>, boost::hash<vector<WordID> > > RuleCRPMap;
  RuleCRPMap rules;
  CCRP_NoTable<vector<WordID> > src_phrases;
  vector<CCRP_NoTable<int> > src_jumps;
};

#endif

struct MyJointModel {
  MyJointModel(PhraseJointBase& rcp0) :
    rp0(rcp0), base(prob_t::One()), rules(1,1), src_jumps(200, CCRP_NoTable<int>(1,1)) {}

  void DecrementRule(const TRule& rule) {
    if (rules.decrement(rule))
      base /= rp0(rule);
  }

  void IncrementRule(const TRule& rule) {
    if (rules.increment(rule))
      base *= rp0(rule);
  }

  void IncrementRules(const vector<TRulePtr>& rules) {
    for (int i = 0; i < rules.size(); ++i)
      IncrementRule(*rules[i]);
  }

  void DecrementRules(const vector<TRulePtr>& rules) {
    for (int i = 0; i < rules.size(); ++i)
      DecrementRule(*rules[i]);
  }

  void IncrementJump(int dist, unsigned src_len) {
    assert(src_len > 0);
    if (src_jumps[src_len].increment(dist))
      base *= jp0(dist, src_len);
  }

  void DecrementJump(int dist, unsigned src_len) {
    assert(src_len > 0);
    if (src_jumps[src_len].decrement(dist))
      base /= jp0(dist, src_len);
  }

  void IncrementJumps(const vector<int>& js, unsigned src_len) {
    for (unsigned i = 0; i < js.size(); ++i)
      IncrementJump(js[i], src_len);
  }

  void DecrementJumps(const vector<int>& js, unsigned src_len) {
    for (unsigned i = 0; i < js.size(); ++i)
      DecrementJump(js[i], src_len);
  }

  // p(jump = dist | src_len , z)
  prob_t JumpProbability(int dist, unsigned src_len) {
    const prob_t p0 = jp0(dist, src_len);
    const double lp = src_jumps[src_len].logprob(dist, log(p0));
    prob_t q; q.logeq(lp);
    return q;
  }

  // p(rule.f_ | z) * p(rule.e_ | rule.f_ , z)
  prob_t RuleProbability(const TRule& rule) const {
    prob_t p; p.logeq(rules.logprob(rule, log(rp0(rule))));
    return p;
  }

  prob_t Likelihood() const {
    prob_t p = base;
    prob_t q; q.logeq(rules.log_crp_prob());
    p *= q;
    for (unsigned l = 1; l < src_jumps.size(); ++l) {
      if (src_jumps[l].num_customers() > 0) {
        prob_t q;
        q.logeq(src_jumps[l].log_crp_prob());
        p *= q;
      }
    }
    return p;
  }

  JumpBase jp0;
  const PhraseJointBase& rp0;
  prob_t base;
  CCRP_NoTable<TRule> rules;
  vector<CCRP_NoTable<int> > src_jumps;
};

struct BackwardEstimate {
  BackwardEstimate(const Model1& m1, const vector<WordID>& src, const vector<WordID>& trg) :
      model1_(m1), src_(src), trg_(trg) {
  }
  const prob_t& operator()(const vector<bool>& src_cov, unsigned trg_cov) const {
    assert(src_.size() == src_cov.size());
    assert(trg_cov <= trg_.size());
    prob_t& e = cache_[src_cov][trg_cov];
    if (e.is_0()) {
      if (trg_cov == trg_.size()) { e = prob_t::One(); return e; }
      vector<WordID> r(src_.size() + 1); r.clear();
      r.push_back(0);  // NULL word
      for (int i = 0; i < src_cov.size(); ++i)
        if (!src_cov[i]) r.push_back(src_[i]);
      const prob_t uniform_alignment(1.0 / r.size());
      e.logeq(Md::log_poisson(trg_.size() - trg_cov, r.size() - 1)); // p(trg len remaining | src len remaining)
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
    }
    return e;
  }
  const Model1& model1_;
  const vector<WordID>& src_;
  const vector<WordID>& trg_;
  mutable unordered_map<vector<bool>, map<unsigned, prob_t>, boost::hash<vector<bool> > > cache_;
};

struct BackwardEstimateSym {
  BackwardEstimateSym(const Model1& m1,
                      const Model1& invm1, const vector<WordID>& src, const vector<WordID>& trg) :
      model1_(m1), invmodel1_(invm1), src_(src), trg_(trg) {
  }
  const prob_t& operator()(const vector<bool>& src_cov, unsigned trg_cov) const {
    assert(src_.size() == src_cov.size());
    assert(trg_cov <= trg_.size());
    prob_t& e = cache_[src_cov][trg_cov];
    if (e.is_0()) {
      if (trg_cov == trg_.size()) { e = prob_t::One(); return e; }
      vector<WordID> r(src_.size() + 1); r.clear();
      for (int i = 0; i < src_cov.size(); ++i)
        if (!src_cov[i]) r.push_back(src_[i]);
      r.push_back(0);  // NULL word
      const prob_t uniform_alignment(1.0 / r.size());
      e.logeq(Md::log_poisson(trg_.size() - trg_cov, r.size() - 1)); // p(trg len remaining | src len remaining)
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
      inv.logeq(Md::log_poisson(r.size(), trg_.size() - trg_cov));
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
  mutable unordered_map<vector<bool>, map<unsigned, prob_t>, boost::hash<vector<bool> > > cache_;
};

struct Particle {
  Particle() : weight(prob_t::One()), src_cov(), trg_cov(), prev_pos(-1) {}
  prob_t weight;
  prob_t gamma_last;
  vector<int> src_jumps;
  vector<TRulePtr> rules;
  vector<bool> src_cv;
  int src_cov;
  int trg_cov;
  int prev_pos;
};

ostream& operator<<(ostream& o, const vector<bool>& v) {
  for (int i = 0; i < v.size(); ++i)
    o << (v[i] ? '1' : '0');
  return o;
}
ostream& operator<<(ostream& o, const Particle& p) {
  o << "[cv=" << p.src_cv << "  src_cov=" << p.src_cov << " trg_cov=" << p.trg_cov << " last_pos=" << p.prev_pos << " num_rules=" << p.rules.size() << "  w=" << log(p.weight) << ']';
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
  ReadParallelCorpus(conf["input"].as<string>(), &corpusf, &corpuse, &vocabf, &vocabe);
  cerr << "F-corpus size: " << corpusf.size() << " sentences\t (" << vocabf.size() << " word types)\n";
  cerr << "E-corpus size: " << corpuse.size() << " sentences\t (" << vocabe.size() << " word types)\n";
  assert(corpusf.size() == corpuse.size());

  const int kLHS = -TD::Convert("X");
  Model1 m1(conf["model1"].as<string>());
  Model1 invm1(conf["inverse_model1"].as<string>());

#if 0
  PhraseConditionalBase lp0(m1, conf["model1_interpolation_weight"].as<double>(), vocabe.size());
  MyConditionalModel m(lp0);
#else
  PhraseJointBase lp0(m1, conf["model1_interpolation_weight"].as<double>(), vocabe.size(), vocabf.size());
  MyJointModel m(lp0);
#endif

  MultinomialResampleFilter<Particle> filter(&rng);
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
  for (int SS=0; SS < samples; ++SS) {
    for (int ci = 0; ci < corpusf.size(); ++ci) {
      vector<int>& src = corpusf[ci];
      vector<int>& trg = corpuse[ci];
      m.DecrementRules(ps[ci].rules);
      m.DecrementJumps(ps[ci].src_jumps, src.size());

      //BackwardEstimate be(m1, src, trg);
      BackwardEstimateSym be(m1, invm1, src, trg);
      const Reachability& r = reaches[ci];
      vector<Particle> lps(particles);

      for (int pi = 0; pi < particles; ++pi) {
        Particle& p = lps[pi];
        p.src_cv.resize(src.size(), false);
      }

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
            int first_uncovered = src.size();
            int last_uncovered = -1;
            for (int i = 0; i < src.size(); ++i) {
              const bool is_uncovered = !p.src_cv[i];
              if (i < first_uncovered && is_uncovered) first_uncovered = i;
              if (is_uncovered && i > last_uncovered) last_uncovered = i;
            }
            assert(last_uncovered > -1);
            assert(first_uncovered < src.size());

            for (int trg_len = 1; trg_len <= kMAX_TRG_PHRASE; ++trg_len) {
              x.e_.push_back(trg[trg_len - 1 + p.trg_cov]);
              for (int src_len = 1; src_len <= kMAX_SRC_PHRASE; ++src_len) {
                if (!r.edges[p.src_cov][p.trg_cov][src_len][trg_len]) continue;

                const int last_possible_start = last_uncovered - src_len + 1;
                assert(last_possible_start >= 0);
                //cerr << src_len << "," << trg_len << " is allowed. E=" << TD::GetString(x.e_) << endl;
                //cerr << "  first_uncovered=" << first_uncovered << "  last_possible_start=" << last_possible_start << endl;
                for (int i = first_uncovered; i <= last_possible_start; ++i) {
                  if (p.src_cv[i]) continue;
                  assert(ss.size() < tmp_p.size());  // if fails increase tmp_p size
                  Particle& np = tmp_p[ss.size()];
                  np = p;
                  x.f_.clear();
                  int gap_add = 0;
                  bool bad = false;
                  prob_t jp = prob_t::One();
                  int prev_pos = p.prev_pos;
                  for (int j = 0; j < src_len; ++j) {
                    if ((j + i + gap_add) == src.size()) { bad = true; break; }
                    while ((i+j+gap_add) < src.size() && p.src_cv[i + j + gap_add]) { ++gap_add; }
                    if ((j + i + gap_add) == src.size()) { bad = true; break; }
                    np.src_cv[i + j + gap_add] = true;
                    x.f_.push_back(src[i + j + gap_add]);
                    jp *= m.JumpProbability(i + j + gap_add - prev_pos, src.size());
                    int jump = i + j + gap_add - prev_pos;
                    assert(jump != 0);
                    np.src_jumps.push_back(jump);
                    prev_pos = i + j + gap_add;
                  }
                  if (bad) continue;
                  np.prev_pos = prev_pos;
                  np.src_cov += x.f_.size();
                  np.trg_cov += x.e_.size();
                  if (x.f_.size() != src_len) continue;
                  prob_t rp = m.RuleProbability(x);
                  np.gamma_last = rp * jp;
                  const prob_t u = pow(np.gamma_last * be(np.src_cv, np.trg_cov), 0.2);
                  //cerr << "**rule=" << x << endl;
                  //cerr << "  u=" << log(u) << "  rule=" << rp << " jump=" << jp << endl;
                  ss.add(u);
                  np.rules.push_back(TRulePtr(new TRule(x)));
                  z += u;

                  const bool completed = (p.trg_cov == trg.size());
                  if (completed) {
                    int last_jump = src.size() - p.prev_pos;
                    assert(last_jump > 0);
                    p.src_jumps.push_back(last_jump);
                    p.weight *= m.JumpProbability(last_jump, src.size());
                  }
                }
              }
            }
            cerr << "number of edges to consider: " << ss.size() << endl;
            const int sampled = rng.SelectSample(ss);
            prob_t q_n = ss[sampled] / z;
            p = tmp_p[sampled];
            //m.IncrementRule(*p.rules.back());
            p.weight *= p.gamma_last / q_n;
            cerr << "[w=" << log(p.weight) << "]\tsampled rule: " << p.rules.back()->AsString() << endl;
            cerr << p << endl;
          }
        } // loop over particles (pi = 0 .. particles)
        if (done_nothing) all_complete = true;
      }
      pfss.clear();
      for (int i = 0; i < lps.size(); ++i)
        pfss.add(lps[i].weight);
      const int sampled = rng.SelectSample(pfss);
      ps[ci] = lps[sampled];
      m.IncrementRules(lps[sampled].rules);
      m.IncrementJumps(lps[sampled].src_jumps, src.size());
      for (int i = 0; i < lps[sampled].rules.size(); ++i) { cerr << "S:\t" << lps[sampled].rules[i]->AsString() << "\n"; }
      cerr << "tmp-LLH: " << log(m.Likelihood()) << endl;
    }
    cerr << "LLH: " << log(m.Likelihood()) << endl;
    for (int sni = 0; sni < 5; ++sni) {
      for (int i = 0; i < ps[sni].rules.size(); ++i) { cerr << "\t" << ps[sni].rules[i]->AsString() << endl; }
    }
  }
  return 0;
}

