#include <iostream>
#include <tr1/memory>
#include <queue>

#include <boost/functional.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "inside_outside.h"
#include "hg.h"
#include "bottom_up_parser.h"
#include "fdict.h"
#include "grammar.h"
#include "m.h"
#include "trule.h"
#include "tdict.h"
#include "filelib.h"
#include "dict.h"
#include "sampler.h"
#include "ccrp.h"
#include "ccrp_onetable.h"

using namespace std;
using namespace tr1;
namespace po = boost::program_options;

boost::shared_ptr<MT19937> prng;
vector<int> nt_vocab;
vector<int> nt_id_to_index;
static unsigned kMAX_RULE_SIZE = 0;
static unsigned kMAX_ARITY = 0;
static bool kALLOW_MIXED = true;  // allow rules with mixed terminals and NTs
static bool kHIERARCHICAL_PRIOR = false;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,s",po::value<unsigned>()->default_value(1000),"Number of samples")
        ("input,i",po::value<string>(),"Read parallel data from")
        ("max_rule_size,m", po::value<unsigned>()->default_value(0), "Maximum rule size (0 for unlimited)")
        ("max_arity,a", po::value<unsigned>()->default_value(0), "Maximum number of nonterminals in a rule (0 for unlimited)")
        ("no_mixed_rules,M", "Do not mix terminals and nonterminals in a rule RHS")
        ("nonterminals,n", po::value<unsigned>()->default_value(1), "Size of nonterminal vocabulary")
        ("hierarchical_prior,h", "Use hierarchical prior")
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

unsigned ReadCorpus(const string& filename,
                    vector<vector<WordID> >* e,
                    set<WordID>* vocab_e) {
  e->clear();
  vocab_e->clear();
  istream* in;
  if (filename == "-")
    in = &cin;
  else
    in = new ifstream(filename.c_str());
  assert(*in);
  string line;
  unsigned toks = 0;
  while(*in) {
    getline(*in, line);
    if (line.empty() && !*in) break;
    e->push_back(vector<int>());
    vector<int>& le = e->back();
    TD::ConvertSentence(line, &le);
    for (unsigned i = 0; i < le.size(); ++i)
      vocab_e->insert(le[i]);
    toks += le.size();
  }
  if (in != &cin) delete in;
  return toks;
}

struct Grid {
  // a b c d e
  // 0 - 0 - -
  vector<int> grid;
};

struct BaseRuleModel {
  explicit BaseRuleModel(unsigned term_size,
                         unsigned nonterm_size = 1) :
      unif_term(1.0 / term_size),
      unif_nonterm(1.0 / nonterm_size) {}
  prob_t operator()(const TRule& r) const {
    prob_t p; p.logeq(Md::log_poisson(1.0, r.f_.size()));
    const prob_t term_prob((2.0 + 0.01*r.f_.size()) / (r.f_.size() + 2));
    const prob_t nonterm_prob(1.0 - term_prob.as_float());
    for (unsigned i = 0; i < r.f_.size(); ++i) {
      if (r.f_[i] <= 0) {     // nonterminal
        if (kALLOW_MIXED) p *= nonterm_prob;
        p *= unif_nonterm;
      } else {                // terminal
        if (kALLOW_MIXED) p *= term_prob;
        p *= unif_term;
      }
    }
    return p;
  }
  const prob_t unif_term, unif_nonterm;
};

struct HieroLMModel {
  explicit HieroLMModel(unsigned vocab_size, unsigned num_nts = 1) :
      base(vocab_size, num_nts),
      q0(1,1,1,1),
      nts(num_nts, CCRP<TRule>(1,1,1,1)) {}

  prob_t Prob(const TRule& r) const {
    return nts[nt_id_to_index[-r.lhs_]].prob(r, p0(r));
  }

  inline prob_t p0(const TRule& r) const {
    if (kHIERARCHICAL_PRIOR)
      return q0.prob(r, base(r));
    else
      return base(r);
  }

  int Increment(const TRule& r, MT19937* rng) {
    const int delta = nts[nt_id_to_index[-r.lhs_]].increment(r, p0(r), rng);
    if (kHIERARCHICAL_PRIOR && delta)
      q0.increment(r, base(r), rng);
    return delta;
    // return x.increment(r);
  }

  int Decrement(const TRule& r, MT19937* rng) {
    const int delta = nts[nt_id_to_index[-r.lhs_]].decrement(r, rng);
    if (kHIERARCHICAL_PRIOR && delta)
      q0.decrement(r, rng);
    return delta;
    //return x.decrement(r);
  }

  prob_t Likelihood() const {
    prob_t p = prob_t::One();
    for (unsigned i = 0; i < nts.size(); ++i) {
      prob_t q; q.logeq(nts[i].log_crp_prob());
      p *= q;
      for (CCRP<TRule>::const_iterator it = nts[i].begin(); it != nts[i].end(); ++it) {
        prob_t tp = p0(it->first);
        tp.poweq(it->second.table_counts_.size());
        p *= tp;
      }
    }
    if (kHIERARCHICAL_PRIOR) {
      prob_t q; q.logeq(q0.log_crp_prob());
      p *= q;
      for (CCRP<TRule>::const_iterator it = q0.begin(); it != q0.end(); ++it) {
        prob_t tp = base(it->first);
        tp.poweq(it->second.table_counts_.size());
        p *= tp;
      }
    }
    //for (CCRP_OneTable<TRule>::const_iterator it = x.begin(); it != x.end(); ++it)
    //    p *= base(it->first);
    return p;
  }

  void ResampleHyperparameters(MT19937* rng) {
    for (unsigned i = 0; i < nts.size(); ++i)
      nts[i].resample_hyperparameters(rng);
    if (kHIERARCHICAL_PRIOR) {
      q0.resample_hyperparameters(rng);
      cerr << "[base d=" << q0.discount() << ", s=" << q0.strength() << "]";
    }
    cerr << " d=" << nts[0].discount() << ", s=" << nts[0].strength() << endl;
  }

  const BaseRuleModel base;
  CCRP<TRule> q0;
  vector<CCRP<TRule> > nts;
  //CCRP_OneTable<TRule> x;
};

vector<GrammarIter* > tofreelist;

HieroLMModel* plm;

struct NPGrammarIter : public GrammarIter, public RuleBin {
  NPGrammarIter() : arity() { tofreelist.push_back(this); }
  NPGrammarIter(const TRulePtr& inr, const int a, int symbol) : arity(a) {
    if (inr) {
      r.reset(new TRule(*inr));
    } else {
      r.reset(new TRule);
    }
    TRule& rr = *r;
    rr.lhs_ = nt_vocab[0];
    rr.f_.push_back(symbol);
    rr.e_.push_back(symbol < 0 ? (1-int(arity)) : symbol);
    tofreelist.push_back(this);
  }
  inline static unsigned NextArity(int cur_a, int symbol) {
    return cur_a + (symbol <= 0 ? 1 : 0);
  }
  virtual int GetNumRules() const {
    if (r) return nt_vocab.size(); else return 0;
  }
  virtual TRulePtr GetIthRule(int i) const {
    if (i == 0) return r;
    TRulePtr nr(new TRule(*r));
    nr->lhs_ = nt_vocab[i];
    return nr;
  }
  virtual int Arity() const {
    return arity;
  }
  virtual const RuleBin* GetRules() const {
    if (!r) return NULL; else return this;
  }
  virtual const GrammarIter* Extend(int symbol) const {
    const int next_arity = NextArity(arity, symbol);
    if (kMAX_ARITY && next_arity > kMAX_ARITY)
      return NULL;
    if (!kALLOW_MIXED && r) {
      bool t1 = r->f_.front() <= 0;
      bool t2 = symbol <= 0;
      if (t1 != t2) return NULL;
    }
    if (!kMAX_RULE_SIZE || !r || (r->f_.size() < kMAX_RULE_SIZE))
      return new NPGrammarIter(r, next_arity, symbol);
    else
      return NULL;
  }
  const unsigned char arity;
  TRulePtr r;
};

struct NPGrammar : public Grammar {
  virtual const GrammarIter* GetRoot() const {
    return new NPGrammarIter;
  }
};

prob_t TotalProb(const Hypergraph& hg) {
  return Inside<prob_t, EdgeProb>(hg);
}

void SampleDerivation(const Hypergraph& hg, MT19937* rng, vector<unsigned>* sampled_deriv) {
  vector<prob_t> node_probs;
  Inside<prob_t, EdgeProb>(hg, &node_probs);
  queue<unsigned> q;
  q.push(hg.nodes_.size() - 2);
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
      //prob_t z;
      assert(num_in_edges > 1);
      SampleSet<prob_t> ss;
      for (unsigned j = 0; j < num_in_edges; ++j) {
        const Hypergraph::Edge& edge = hg.edges_[node.in_edges_[j]];
        prob_t p = edge.edge_prob_;
        for (unsigned k = 0; k < edge.tail_nodes_.size(); ++k)
          p *= node_probs[edge.tail_nodes_[k]];
        ss.add(p);
//        cerr << log(ss[j]) << " ||| " << edge.rule_->AsString() << endl;
        //z += p;
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
  for (unsigned i = 0; i < sampled_deriv->size(); ++i) {
    cerr << *hg.edges_[(*sampled_deriv)[i]].rule_ << endl;
  }
}

void IncrementDerivation(const Hypergraph& hg, const vector<unsigned>& d, HieroLMModel* plm, MT19937* rng) {
  for (unsigned i = 0; i < d.size(); ++i)
    plm->Increment(*hg.edges_[d[i]].rule_, rng);
}

void DecrementDerivation(const Hypergraph& hg, const vector<unsigned>& d, HieroLMModel* plm, MT19937* rng) {
  for (unsigned i = 0; i < d.size(); ++i)
    plm->Decrement(*hg.edges_[d[i]].rule_, rng);
}

int main(int argc, char** argv) {
  po::variables_map conf;

  InitCommandLine(argc, argv, &conf);
  nt_vocab.resize(conf["nonterminals"].as<unsigned>());
  assert(nt_vocab.size() > 0);
  assert(nt_vocab.size() < 26);
  {
    string nt = "X";
    for (unsigned i = 0; i < nt_vocab.size(); ++i) {
      if (nt_vocab.size() > 1) nt[0] = ('A' + i);
      int pid = TD::Convert(nt);
      nt_vocab[i] = -pid;
      if (pid >= nt_id_to_index.size()) {
        nt_id_to_index.resize(pid + 1, -1);
      }
      nt_id_to_index[pid] = i;
    }
  }
  vector<GrammarPtr> grammars;
  grammars.push_back(GrammarPtr(new NPGrammar));

  const unsigned samples = conf["samples"].as<unsigned>();
  kMAX_RULE_SIZE = conf["max_rule_size"].as<unsigned>();
  if (kMAX_RULE_SIZE == 1) {
    cerr << "Invalid maximum rule size: must be 0 or >1\n";
    return 1;
  }
  kMAX_ARITY = conf["max_arity"].as<unsigned>();
  if (kMAX_ARITY == 1) {
    cerr << "Invalid maximum arity: must be 0 or >1\n";
    return 1;
  }
  kALLOW_MIXED = !conf.count("no_mixed_rules");

  kHIERARCHICAL_PRIOR = conf.count("hierarchical_prior");

  if (conf.count("random_seed"))
    prng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    prng.reset(new MT19937);
  MT19937& rng = *prng;
  vector<vector<WordID> > corpuse;
  set<WordID> vocabe;
  cerr << "Reading corpus...\n";
  const unsigned toks = ReadCorpus(conf["input"].as<string>(), &corpuse, &vocabe);
  cerr << "E-corpus size: " << corpuse.size() << " sentences\t (" << vocabe.size() << " word types)\n";
  HieroLMModel lm(vocabe.size(), nt_vocab.size());

  plm = &lm;
  ExhaustiveBottomUpParser parser(TD::Convert(-nt_vocab[0]), grammars);

  Hypergraph hg;
  const int kGoal = -TD::Convert("Goal");
  const int kLP = FD::Convert("LogProb");
  SparseVector<double> v; v.set_value(kLP, 1.0);
  vector<vector<unsigned> > derivs(corpuse.size());
  vector<Lattice> cl(corpuse.size());
  for (int ci = 0; ci < corpuse.size(); ++ci) {
    vector<int>& src = corpuse[ci];
    Lattice& lat = cl[ci];
    lat.resize(src.size());
    for (unsigned i = 0; i < src.size(); ++i)
      lat[i].push_back(LatticeArc(src[i], 0.0, 1));
  }
  for (int SS=0; SS < samples; ++SS) {
    const bool is_last = ((samples - 1) == SS);
    prob_t dlh = prob_t::One();
    for (int ci = 0; ci < corpuse.size(); ++ci) {
      const vector<int>& src = corpuse[ci];
      const Lattice& lat = cl[ci];
      cerr << TD::GetString(src) << endl;
      hg.clear();
      parser.Parse(lat, &hg);  // exhaustive parse
      vector<unsigned>& d = derivs[ci];
      if (!is_last) DecrementDerivation(hg, d, &lm, &rng);
      for (unsigned i = 0; i < hg.edges_.size(); ++i) {
        TRule& r = *hg.edges_[i].rule_;
        if (r.lhs_ == kGoal)
          hg.edges_[i].edge_prob_ = prob_t::One();
        else
          hg.edges_[i].edge_prob_ = lm.Prob(r);
      }
      if (!is_last) {
        d.clear();
        SampleDerivation(hg, &rng, &d);
        IncrementDerivation(hg, derivs[ci], &lm, &rng);
      } else {
        prob_t p = TotalProb(hg);
        dlh *= p;
        cerr << " p(sentence) = " << log(p) << "\t" << log(dlh) << endl;
      }
      if (tofreelist.size() > 200000) {
        cerr << "Freeing ... ";
        for (unsigned i = 0; i < tofreelist.size(); ++i)
          delete tofreelist[i];
        tofreelist.clear();
        cerr << "Freed.\n";
      }
    }
    double llh = log(lm.Likelihood());
    cerr << "LLH=" << llh << "\tENTROPY=" << (-llh / log(2) / toks) << "\tPPL=" << pow(2, -llh / log(2) / toks) << endl;
    if (SS % 10 == 9) lm.ResampleHyperparameters(&rng);
    if (is_last) {
      double z = log(dlh);
      cerr << "TOTAL_PROB=" << z << "\tENTROPY=" << (-z / log(2) / toks) << "\tPPL=" << pow(2, -z / log(2) / toks) << endl;
    }
  }
  for (unsigned i = 0; i < nt_vocab.size(); ++i)
    cerr << lm.nts[i] << endl;
  return 0;
}

