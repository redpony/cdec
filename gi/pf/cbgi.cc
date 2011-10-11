#include <queue>
#include <sstream>
#include <iostream>

#include <boost/unordered_map.hpp>
#include <boost/functional/hash.hpp>

#include "sampler.h"
#include "filelib.h"
#include "hg_io.h"
#include "hg.h"
#include "ccrp_nt.h"
#include "trule.h"
#include "inside_outside.h"

using namespace std;
using namespace std::tr1;

double log_poisson(unsigned x, const double& lambda) {
  assert(lambda > 0.0);
  return log(lambda) * x - lgamma(x + 1) - lambda;
}

double log_decay(unsigned x, const double& b) {
  assert(b > 1.0);
  assert(x > 0);
  return log(b - 1) - x * log(b);
}

struct SimpleBase {
  SimpleBase(unsigned esize, unsigned fsize, unsigned ntsize = 144) :
    uniform_e(-log(esize)),
    uniform_f(-log(fsize)),
    uniform_nt(-log(ntsize)) {
  }

  // binomial coefficient
  static double choose(unsigned n, unsigned k) {
    return exp(lgamma(n + 1) - lgamma(k + 1) - lgamma(n - k + 1));
  }

  // count the number of patterns of terminals and NTs in the rule, given elen and flen
  static double log_number_of_patterns(const unsigned flen, const unsigned elen) {
    static vector<vector<double> > counts;
    if (elen >= counts.size()) counts.resize(elen + 1);
    if (flen >= counts[elen].size()) counts[elen].resize(flen + 1);
    double& count = counts[elen][flen];
    if (count) return log(count);
    const unsigned max_arity = min(elen, flen);
    for (unsigned a = 0; a <= max_arity; ++a)
      count += choose(elen, a) * choose(flen, a);
    return log(count);
  }

  // return logp0 of rule | LHS
  double operator()(const TRule& rule) const {
    const unsigned flen = rule.f_.size();
    const unsigned elen = rule.e_.size();
#if 0
    double p = 0;
    p += log_poisson(flen, 0.5);                   // flen                 ~Pois(0.5)
    p += log_poisson(elen, flen);                  // elen | flen          ~Pois(flen)
    p -= log_number_of_patterns(flen, elen);       // pattern | flen,elen  ~Uniform
    for (unsigned i = 0; i < flen; ++i) {          // for each position in f-RHS
      if (rule.f_[i] <= 0)                         //   according to pattern
        p += uniform_nt;                           //     draw NT          ~Uniform
      else
        p += uniform_f;                            //     draw f terminal  ~Uniform
    }
    p -= lgamma(rule.Arity() + 1);                 // draw permutation     ~Uniform 
    for (unsigned i = 0; i < elen; ++i) {          // for each position in e-RHS
      if (rule.e_[i] > 0)                          //   according to pattern
        p += uniform_e;                            //     draw e|f term    ~Uniform
        // TODO this should prob be model 1
    }
#else
    double p = 0;
    bool is_abstract = rule.f_[0] <= 0;
    p += log(0.5);
    if (is_abstract) {
      if (flen == 2) p += log(0.99); else p += log(0.01);
    } else {
      p += log_decay(flen, 3);
    }

    for (unsigned i = 0; i < flen; ++i) {          // for each position in f-RHS
      if (rule.f_[i] <= 0)                         //   according to pattern
        p += uniform_nt;                           //     draw NT          ~Uniform
      else
        p += uniform_f;                            //     draw f terminal  ~Uniform
    }
#endif
    return p;
  }
  const double uniform_e;
  const double uniform_f;
  const double uniform_nt;
  vector<double> arities;
};

MT19937* rng = NULL;

template <typename Base>
struct MHSamplerEdgeProb {
  MHSamplerEdgeProb(const Hypergraph& hg,
                  const map<int, CCRP_NoTable<TRule> >& rdp,
                  const Base& logp0,
                  const bool exclude_multiword_terminals) : edge_probs(hg.edges_.size()) {
    for (int i = 0; i < edge_probs.size(); ++i) {
      const TRule& rule = *hg.edges_[i].rule_;
      const map<int, CCRP_NoTable<TRule> >::const_iterator it = rdp.find(rule.lhs_);
      assert(it != rdp.end());
      const CCRP_NoTable<TRule>& crp = it->second;
      edge_probs[i].logeq(crp.logprob(rule, logp0(rule)));
      if (exclude_multiword_terminals && rule.f_[0] > 0 && rule.f_.size() > 1)
        edge_probs[i] = prob_t::Zero();
    }
  }
  inline prob_t operator()(const Hypergraph::Edge& e) const {
    return edge_probs[e.id_];
  }
  prob_t DerivationProb(const vector<int>& d) const {
    prob_t p = prob_t::One();
    for (unsigned i = 0; i < d.size(); ++i)
      p *= edge_probs[d[i]];
    return p;
  }
  vector<prob_t> edge_probs;
};

template <typename Base>
struct ModelAndData {
  ModelAndData() :
     base_lh(prob_t::One()),
     logp0(10000, 10000),
     mh_samples(),
     mh_rejects() {}

  void SampleCorpus(const string& hgpath, int i);
  void ResampleHyperparameters() {
    for (map<int, CCRP_NoTable<TRule> >::iterator it = rules.begin(); it != rules.end(); ++it)
      it->second.resample_hyperparameters(rng);
  }

  CCRP_NoTable<TRule>& RuleCRP(int lhs) {
    map<int, CCRP_NoTable<TRule> >::iterator it = rules.find(lhs);
    if (it == rules.end()) {
      rules.insert(make_pair(lhs, CCRP_NoTable<TRule>(1,1)));
      it = rules.find(lhs);
    }
    return it->second;
  }

  void IncrementRule(const TRule& rule) {
    CCRP_NoTable<TRule>& crp = RuleCRP(rule.lhs_);
    if (crp.increment(rule)) {
      prob_t p; p.logeq(logp0(rule));
      base_lh *= p;
    }
  }

  void DecrementRule(const TRule& rule) {
    CCRP_NoTable<TRule>& crp = RuleCRP(rule.lhs_);
    if (crp.decrement(rule)) {
      prob_t p; p.logeq(logp0(rule));
      base_lh /= p;
    }
  }

  void DecrementDerivation(const Hypergraph& hg, const vector<int>& d) {
    for (unsigned i = 0; i < d.size(); ++i) {
      const TRule& rule = *hg.edges_[d[i]].rule_;
      DecrementRule(rule);
    }
  }

  void IncrementDerivation(const Hypergraph& hg, const vector<int>& d) {
    for (unsigned i = 0; i < d.size(); ++i) {
      const TRule& rule = *hg.edges_[d[i]].rule_;
      IncrementRule(rule);
    }
  }

  prob_t Likelihood() const {
    prob_t p = prob_t::One();
    for (map<int, CCRP_NoTable<TRule> >::const_iterator it = rules.begin(); it != rules.end(); ++it) {
      prob_t q; q.logeq(it->second.log_crp_prob());
      p *= q;
    }
    p *= base_lh;
    return p;
  }

  void ResampleDerivation(const Hypergraph& hg, vector<int>* sampled_derivation);

  map<int, CCRP_NoTable<TRule> > rules;  // [lhs] -> distribution over RHSs
  prob_t base_lh;
  SimpleBase logp0;
  vector<vector<int> > samples;   // sampled derivations
  unsigned int mh_samples;
  unsigned int mh_rejects;
};

template <typename Base>
void ModelAndData<Base>::SampleCorpus(const string& hgpath, int n) {
  vector<Hypergraph> hgs(n); hgs.clear();
  boost::unordered_map<TRule, unsigned> acc;
  map<int, unsigned> tot;
  for (int i = 0; i < n; ++i) {
    ostringstream os;
    os << hgpath << '/' << i << ".json.gz";
    if (!FileExists(os.str())) continue;
    hgs.push_back(Hypergraph());
    ReadFile rf(os.str());
    HypergraphIO::ReadFromJSON(rf.stream(), &hgs.back());
  }
  cerr << "Read " << hgs.size() << " alignment hypergraphs.\n";
  samples.resize(hgs.size());
  const unsigned SAMPLES = 2000;
  const unsigned burnin = 3 * SAMPLES / 4;
  const unsigned every = 20;
  for (unsigned s = 0; s < SAMPLES; ++s) {
    if (s % 10 == 0) {
      if (s > 0) { cerr << endl; ResampleHyperparameters(); }
      cerr << "[" << s << " LLH=" << log(Likelihood()) << " REJECTS=" << ((double)mh_rejects / mh_samples) << " LHS's=" << rules.size() << " base=" << log(base_lh) << "] ";
    }
    cerr << '.';
    for (unsigned i = 0; i < hgs.size(); ++i) {
      ResampleDerivation(hgs[i], &samples[i]);
      if (s > burnin && s % every == 0) {
        for (unsigned j = 0; j < samples[i].size(); ++j) {
          const TRule& rule = *hgs[i].edges_[samples[i][j]].rule_;
          ++acc[rule];
          ++tot[rule.lhs_];
        }
      }
    }
  }
  cerr << endl;
  for (boost::unordered_map<TRule,unsigned>::iterator it = acc.begin(); it != acc.end(); ++it) {
    cout << it->first << " MyProb=" << log(it->second)-log(tot[it->first.lhs_]) << endl;
  }
}

template <typename Base>
void ModelAndData<Base>::ResampleDerivation(const Hypergraph& hg, vector<int>* sampled_deriv) {
  vector<int> cur;
  cur.swap(*sampled_deriv);

  const prob_t p_cur = Likelihood();
  DecrementDerivation(hg, cur);
  if (cur.empty()) {
    // first iteration, create restaurants
    for (int i = 0; i < hg.edges_.size(); ++i)
      RuleCRP(hg.edges_[i].rule_->lhs_);
  }
  MHSamplerEdgeProb<SimpleBase> wf(hg, rules, logp0, cur.empty());
//  MHSamplerEdgeProb<SimpleBase> wf(hg, rules, logp0, false);
  const prob_t q_cur = wf.DerivationProb(cur);
  vector<prob_t> node_probs;
  Inside<prob_t, MHSamplerEdgeProb<SimpleBase> >(hg, &node_probs, wf);
  queue<unsigned> q;
  q.push(hg.nodes_.size() - 3);
  while(!q.empty()) {
    unsigned cur_node_id = q.front();
//    cerr << "NODE=" << cur_node_id << endl;
    q.pop();
    const Hypergraph::Node& node = hg.nodes_[cur_node_id];
    const unsigned num_in_edges = node.in_edges_.size();
    unsigned sampled_edge = 0;
    if (num_in_edges == 1) {
      sampled_edge = node.in_edges_[0];
    } else {
      prob_t z;
      assert(num_in_edges > 1);
      SampleSet<prob_t> ss;
      for (unsigned j = 0; j < num_in_edges; ++j) {
        const Hypergraph::Edge& edge = hg.edges_[node.in_edges_[j]];
        prob_t p = wf.edge_probs[edge.id_];             // edge proposal prob
        for (unsigned k = 0; k < edge.tail_nodes_.size(); ++k)
          p *= node_probs[edge.tail_nodes_[k]];
        ss.add(p);
//        cerr << log(ss[j]) << " ||| " << edge.rule_->AsString() << endl;
        z += p;
      }
//      for (unsigned j = 0; j < num_in_edges; ++j) {
//        const Hypergraph::Edge& edge = hg.edges_[node.in_edges_[j]];
//        cerr << exp(log(ss[j] / z)) << " ||| " << edge.rule_->AsString() << endl;
//      }
//      cerr << " --- \n";
      sampled_edge = node.in_edges_[rng->SelectSample(ss)];
    }
    sampled_deriv->push_back(sampled_edge);
    const Hypergraph::Edge& edge = hg.edges_[sampled_edge];
    for (unsigned j = 0; j < edge.tail_nodes_.size(); ++j) {
      q.push(edge.tail_nodes_[j]);
    }
  }
  IncrementDerivation(hg, *sampled_deriv);

//  cerr << "sampled derivation contains " << sampled_deriv->size() << " edges\n";
//  cerr << "DERIV:\n";
//  for (int i = 0; i < sampled_deriv->size(); ++i) {
//    cerr << "  " << hg.edges_[(*sampled_deriv)[i]].rule_->AsString() << endl;
//  }

  if (cur.empty()) return;  // accept first sample

  ++mh_samples;
  // only need to do MH if proposal is different to current state
  if (cur != *sampled_deriv) {
    const prob_t q_prop = wf.DerivationProb(*sampled_deriv);
    const prob_t p_prop = Likelihood();
    if (!rng->AcceptMetropolisHastings(p_prop, p_cur, q_prop, q_cur)) {
      ++mh_rejects;
      DecrementDerivation(hg, *sampled_deriv);
      IncrementDerivation(hg, cur);
      swap(cur, *sampled_deriv);
    }
  }
}

int main(int argc, char** argv) {
  rng = new MT19937;
  ModelAndData<SimpleBase> m;
  m.SampleCorpus("./hgs", 50);
  // m.SampleCorpus("./btec/hgs", 5000);
  return 0;
}

