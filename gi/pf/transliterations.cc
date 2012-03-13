#include "transliterations.h"

#include <iostream>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "backward.h"
#include "filelib.h"
#include "tdict.h"
#include "trule.h"
#include "filelib.h"
#include "ccrp_nt.h"
#include "m.h"
#include "reachability.h"

using namespace std;
using namespace std::tr1;

struct TruncatedConditionalLengthModel {
  TruncatedConditionalLengthModel(unsigned max_src_size, unsigned max_trg_size, double expected_src_to_trg_ratio) :
      plens(max_src_size+1, vector<prob_t>(max_trg_size+1, 0.0)) {
    for (unsigned i = 1; i <= max_src_size; ++i) {
      prob_t z = prob_t::Zero();
      for (unsigned j = 1; j <= max_trg_size; ++j)
        z += (plens[i][j] = prob_t(0.01 + exp(Md::log_poisson(j, i * expected_src_to_trg_ratio))));
      for (unsigned j = 1; j <= max_trg_size; ++j)
        plens[i][j] /= z;
      //for (unsigned j = 1; j <= max_trg_size; ++j)
      //  cerr << "P(trg_len=" << j << " | src_len=" << i << ") = " << plens[i][j] << endl;
    }
  }

  // return p(tlen | slen) for *chunks* not full words
  inline const prob_t& operator()(int slen, int tlen) const {
    return plens[slen][tlen];
  }

  vector<vector<prob_t> > plens;
};

struct CondBaseDist {
  CondBaseDist(unsigned max_src_size, unsigned max_trg_size, double expected_src_to_trg_ratio) :
    tclm(max_src_size, max_trg_size, expected_src_to_trg_ratio) {}

  prob_t operator()(const vector<WordID>& src, unsigned sf, unsigned st,
                    const vector<WordID>& trg, unsigned tf, unsigned tt) const {
    prob_t p = tclm(st - sf, tt - tf);  // target len | source length ~ TCLM(source len)
    assert(!"not impl");
    return p;
  }
  inline prob_t operator()(const vector<WordID>& src, const vector<WordID>& trg) const {
    return (*this)(src, 0, src.size(), trg, 0, trg.size());
  }
  TruncatedConditionalLengthModel tclm;
};

// represents transliteration phrase probabilities, e.g.
//   p( a l - | A l ) , p( o | A w ) , ...
struct TransliterationChunkConditionalModel {
  explicit TransliterationChunkConditionalModel(const CondBaseDist& pp0) :
      d(0.0),
      strength(1.0),
      rp0(pp0) {
  }

  void Summary() const {
    std::cerr << "Number of conditioning contexts: " << r.size() << std::endl;
    for (RuleModelHash::const_iterator it = r.begin(); it != r.end(); ++it) {
      std::cerr << TD::GetString(it->first) << "   \t(\\alpha = " << it->second.alpha() << ") --------------------------" << std::endl;
      for (CCRP_NoTable<TRule>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
        std::cerr << "   " << i2->second << '\t' << i2->first << std::endl;
    }
  }

  int DecrementRule(const TRule& rule) {
    RuleModelHash::iterator it = r.find(rule.f_);
    assert(it != r.end());    
    int count = it->second.decrement(rule);
    if (count) {
      if (it->second.num_customers() == 0) r.erase(it);
    }
    return count;
  }

  int IncrementRule(const TRule& rule) {
    RuleModelHash::iterator it = r.find(rule.f_);
    if (it == r.end()) {
      it = r.insert(make_pair(rule.f_, CCRP_NoTable<TRule>(strength))).first;
    } 
    int count = it->second.increment(rule);
    return count;
  }

  void IncrementRules(const std::vector<TRulePtr>& rules) {
    for (int i = 0; i < rules.size(); ++i)
      IncrementRule(*rules[i]);
  }

  void DecrementRules(const std::vector<TRulePtr>& rules) {
    for (int i = 0; i < rules.size(); ++i)
      DecrementRule(*rules[i]);
  }

  prob_t RuleProbability(const TRule& rule) const {
    prob_t p;
    RuleModelHash::const_iterator it = r.find(rule.f_);
    if (it == r.end()) {
      p = rp0(rule.f_, rule.e_);
    } else {
      p = it->second.prob(rule, rp0(rule.f_, rule.e_));
    }
    return p;
  }

  double LogLikelihood(const double& dd, const double& aa) const {
    if (aa <= -dd) return -std::numeric_limits<double>::infinity();
    //double llh = Md::log_beta_density(dd, 10, 3) + Md::log_gamma_density(aa, 1, 1);
    double llh = //Md::log_beta_density(dd, 1, 1) +
                 Md::log_gamma_density(dd + aa, 1, 1);
    typename std::tr1::unordered_map<std::vector<WordID>, CCRP_NoTable<TRule>, boost::hash<std::vector<WordID> > >::const_iterator it;
    for (it = r.begin(); it != r.end(); ++it)
      llh += it->second.log_crp_prob(aa);
    return llh;
  }

  struct AlphaResampler {
    AlphaResampler(const TransliterationChunkConditionalModel& m) : m_(m) {}
    const TransliterationChunkConditionalModel& m_;
    double operator()(const double& proposed_strength) const {
      return m_.LogLikelihood(m_.d, proposed_strength);
    }
  };

  void ResampleHyperparameters(MT19937* rng) {
    typename std::tr1::unordered_map<std::vector<WordID>, CCRP_NoTable<TRule>, boost::hash<std::vector<WordID> > >::iterator it;
    //const unsigned nloop = 5;
    const unsigned niterations = 10;
    //DiscountResampler dr(*this);
    AlphaResampler ar(*this);
#if 0
    for (int iter = 0; iter < nloop; ++iter) {
      strength = slice_sampler1d(ar, strength, *rng, -d + std::numeric_limits<double>::min(),
                              std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
      double min_discount = std::numeric_limits<double>::min();
      if (strength < 0.0) min_discount -= strength;
      d = slice_sampler1d(dr, d, *rng, min_discount,
                          1.0, 0.0, niterations, 100*niterations);
    }
#endif
    strength = slice_sampler1d(ar, strength, *rng, -d,
                            std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
    std::cerr << "CTMModel(alpha=" << strength << ") = " << LogLikelihood(d, strength) << std::endl;
    for (it = r.begin(); it != r.end(); ++it) {
#if 0
      it->second.set_discount(d);
#endif
      it->second.set_alpha(strength);
    }
  }

  prob_t Likelihood() const {
    prob_t p; p.logeq(LogLikelihood(d, strength));
    return p;
  }

  const CondBaseDist& rp0;
  typedef std::tr1::unordered_map<std::vector<WordID>,
                                  CCRP_NoTable<TRule>,
                                  boost::hash<std::vector<WordID> > > RuleModelHash;
  RuleModelHash r;
  double d, strength;
};

struct GraphStructure {
  GraphStructure() : r() {}
  // leak memory - these are basically static
  const Reachability* r;
  bool IsReachable() const { return r->nodes > 0; }
};

struct ProbabilityEstimates {
  ProbabilityEstimates() : gs(), backward() {}
  explicit ProbabilityEstimates(const GraphStructure& g) :
      gs(&g), backward() {
    if (g.r->nodes > 0)
      backward = new float[g.r->nodes];
  }
  // leak memory, these are static

  // returns an estimate of the marginal probability
  double MarginalEstimate() const {
    if (!backward) return 0;
    return backward[0];
  }

  // returns an backward estimate
  double Backward(int src_covered, int trg_covered) const {
    if (!backward) return 0;
    int ind = gs->r->node_addresses[src_covered][trg_covered];
    if (ind < 0) return 0;
    return backward[ind];
  }

  prob_t estp;
  float* backward;
 private:
  const GraphStructure* gs;
};

struct TransliterationsImpl {
  TransliterationsImpl(int max_src, int max_trg, double sr, const BackwardEstimator& b) :
      cp0(max_src, max_trg, sr),
      tccm(cp0),
      be(b),
      kMAX_SRC_CHUNK(max_src),
      kMAX_TRG_CHUNK(max_trg),
      kS2T_RATIO(sr),
      tot_pairs(), tot_mem() {
  }
  const CondBaseDist cp0;
  TransliterationChunkConditionalModel tccm;
  const BackwardEstimator& be;

  void Initialize(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
    const size_t src_len = src_lets.size();
    const size_t trg_len = trg_lets.size();

    // init graph structure
    if (src_len >= graphs.size()) graphs.resize(src_len + 1);
    if (trg_len >= graphs[src_len].size()) graphs[src_len].resize(trg_len + 1);
    GraphStructure& gs = graphs[src_len][trg_len];
    if (!gs.r) {
      double rat = exp(fabs(log(trg_len / (src_len * kS2T_RATIO))));
      if (rat > 1.5 || (rat > 2.4 && src_len < 6)) {
        cerr << " ** Forbidding transliterations of size " << src_len << "," << trg_len << ": " << rat << endl;
        gs.r = new Reachability(src_len, trg_len, 0, 0);
      } else {
        gs.r = new Reachability(src_len, trg_len, kMAX_SRC_CHUNK, kMAX_TRG_CHUNK);
      }
    }

    const Reachability& r = *gs.r;

    // init backward estimates
    if (src >= ests.size()) ests.resize(src + 1);
    unordered_map<WordID, ProbabilityEstimates>::iterator it = ests[src].find(trg);
    if (it != ests[src].end()) return; // already initialized

    it = ests[src].insert(make_pair(trg, ProbabilityEstimates(gs))).first;
    ProbabilityEstimates& est = it->second;
    if (!gs.r->nodes) return;  // not derivable subject to length constraints

    be.InitializeGrid(src_lets, trg_lets, r, kS2T_RATIO, est.backward);
    cerr << TD::GetString(src_lets) << " ||| " << TD::GetString(trg_lets) << " ||| " << (est.backward[0] / trg_lets.size()) << endl;
    tot_pairs++;
    tot_mem += sizeof(float) * gs.r->nodes;
  }

  void Forbid(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
    const size_t src_len = src_lets.size();
    const size_t trg_len = trg_lets.size();
    // TODO
  }

  prob_t EstimateProbability(WordID s, const vector<WordID>& src, WordID t, const vector<WordID>& trg) const {
    assert(src.size() < graphs.size());
    const vector<GraphStructure>& tv = graphs[src.size()];
    assert(trg.size() < tv.size());
    const GraphStructure& gs = tv[trg.size()];
    if (gs.r->nodes == 0)
      return prob_t::Zero();
    const unordered_map<WordID, ProbabilityEstimates>::const_iterator it = ests[s].find(t);
    assert(it != ests[s].end());
    return it->second.estp;
  }

  void GraphSummary() const {
    double to = 0;
    double tn = 0;
    double tt = 0;
    for (int i = 0; i < graphs.size(); ++i) {
      const vector<GraphStructure>& vt = graphs[i];
      for (int j = 0; j < vt.size(); ++j) {
        const GraphStructure& gs = vt[j];
        if (!gs.r) continue;
        tt++;
        for (int k = 0; k < i; ++k) {
          for (int l = 0; l < j; ++l) {
            size_t c = gs.r->valid_deltas[k][l].size();
            if (c) {
              tn += 1;
              to += c;
            }
          }
        }
      }
    }
    cerr << "     Average nodes = " << (tn / tt) << endl;
    cerr << "Average out-degree = " << (to / tn) << endl;
    cerr << " Unique structures = " << tt << endl;
    cerr << "      Unique pairs = " << tot_pairs << endl;
    cerr << "          BEs size = " << (tot_mem / (1024.0*1024.0)) << " MB" << endl;
  }

  const int kMAX_SRC_CHUNK;
  const int kMAX_TRG_CHUNK;
  const double kS2T_RATIO;
  unsigned tot_pairs;
  size_t tot_mem;
  vector<vector<GraphStructure> > graphs; // graphs[src_len][trg_len]
  vector<unordered_map<WordID, ProbabilityEstimates> > ests; // ests[src][trg]
};

Transliterations::Transliterations(int max_src, int max_trg, double sr, const BackwardEstimator& be) :
    pimpl_(new TransliterationsImpl(max_src, max_trg, sr, be)) {}
Transliterations::~Transliterations() { delete pimpl_; }

void Transliterations::Initialize(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
  pimpl_->Initialize(src, src_lets, trg, trg_lets);
}

prob_t Transliterations::EstimateProbability(WordID s, const vector<WordID>& src, WordID t, const vector<WordID>& trg) const {
  return pimpl_->EstimateProbability(s, src,t, trg);
}

void Transliterations::Forbid(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
  pimpl_->Forbid(src, src_lets, trg, trg_lets);
}

void Transliterations::GraphSummary() const {
  pimpl_->GraphSummary();
}

