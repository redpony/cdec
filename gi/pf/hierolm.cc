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

shared_ptr<MT19937> prng;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,s",po::value<unsigned>()->default_value(1000),"Number of samples")
        ("input,i",po::value<string>(),"Read parallel data from")
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

void ReadCorpus(const string& filename,
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
  while(*in) {
    getline(*in, line);
    if (line.empty() && !*in) break;
    e->push_back(vector<int>());
    vector<int>& le = e->back();
    TD::ConvertSentence(line, &le);
    for (unsigned i = 0; i < le.size(); ++i)
      vocab_e->insert(le[i]);
  }
  if (in != &cin) delete in;
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
        p *= nonterm_prob;
        p *= unif_nonterm;
      } else {                // terminal
        p *= term_prob;
        p *= unif_term;
      }
    }
    return p;
  }
  const prob_t unif_term, unif_nonterm;
};

struct HieroLMModel {
  explicit HieroLMModel(unsigned vocab_size) : p0(vocab_size), x(1,1,1,1) {}

  prob_t Prob(const TRule& r) const {
    return x.probT<prob_t>(r, p0(r));
  }

  int Increment(const TRule& r, MT19937* rng) {
    return x.incrementT<prob_t>(r, p0(r), rng);
    // return x.increment(r);
  }

  int Decrement(const TRule& r, MT19937* rng) {
    return x.decrement(r, rng);
    //return x.decrement(r);
  }

  prob_t Likelihood() const {
    prob_t p;
    p.logeq(x.log_crp_prob());
    for (CCRP<TRule>::const_iterator it = x.begin(); it != x.end(); ++it) {
      prob_t tp = p0(it->first);
      tp.poweq(it->second.table_counts_.size());
      p *= tp;
    }
    //for (CCRP_OneTable<TRule>::const_iterator it = x.begin(); it != x.end(); ++it)
    //    p *= p0(it->first);
    return p;
  }

  void ResampleHyperparameters(MT19937* rng) {
    x.resample_hyperparameters(rng);
    cerr << " d=" << x.discount() << ", alpha=" << x.concentration() << endl;
  }

  const BaseRuleModel p0;
  CCRP<TRule> x;
  //CCRP_OneTable<TRule> x;
};

vector<GrammarIter* > tofreelist;

HieroLMModel* plm;

struct NPGrammarIter : public GrammarIter, public RuleBin {
  NPGrammarIter() : arity() { tofreelist.push_back(this); }
  NPGrammarIter(const TRulePtr& inr, const int a, int symbol) : arity(a + (symbol < 0 ? 1 : 0)) {
    if (inr) {
      r.reset(new TRule(*inr));
    } else {
      static const int kLHS = -TD::Convert("X");
      r.reset(new TRule);
      r->lhs_ = kLHS;
    }
    TRule& rr = *r;
    rr.f_.push_back(symbol);
    rr.e_.push_back(symbol < 0 ? (1-int(arity)) : symbol);
    tofreelist.push_back(this);
  }
  virtual int GetNumRules() const {
    if (r) return 1; else return 0;
  }
  virtual TRulePtr GetIthRule(int) const {
    return r;
  }
  virtual int Arity() const {
    return arity;
  }
  virtual const RuleBin* GetRules() const {
    if (!r) return NULL; else return this;
  }
  virtual const GrammarIter* Extend(int symbol) const {
    return new NPGrammarIter(r, arity, symbol);
  }
  const unsigned char arity;
  TRulePtr r;
};

struct NPGrammar : public Grammar {
  virtual const GrammarIter* GetRoot() const {
    return new NPGrammarIter;
  }
};

void SampleDerivation(const Hypergraph& hg, MT19937* rng, vector<unsigned>* sampled_deriv, HieroLMModel* plm) {
  HieroLMModel& lm = *plm;
  vector<prob_t> node_probs;
  const prob_t total_prob = Inside<prob_t, EdgeProb>(hg, &node_probs);
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
  vector<GrammarPtr> grammars;
  grammars.push_back(GrammarPtr(new NPGrammar));

  InitCommandLine(argc, argv, &conf);
  const unsigned samples = conf["samples"].as<unsigned>();

  if (conf.count("random_seed"))
    prng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    prng.reset(new MT19937);
  MT19937& rng = *prng;

  vector<vector<WordID> > corpuse;
  set<WordID> vocabe;
  cerr << "Reading corpus...\n";
  ReadCorpus(conf["input"].as<string>(), &corpuse, &vocabe);
  cerr << "E-corpus size: " << corpuse.size() << " sentences\t (" << vocabe.size() << " word types)\n";
  HieroLMModel lm(vocabe.size());

  plm = &lm;
  ExhaustiveBottomUpParser parser("X", grammars);

  Hypergraph hg;
  const int kX = -TD::Convert("X");
  const int kLP = FD::Convert("LogProb");
  SparseVector<double> v; v.set_value(kLP, 1.0);
  vector<vector<unsigned> > derivs(corpuse.size());
  for (int SS=0; SS < samples; ++SS) {
    for (int ci = 0; ci < corpuse.size(); ++ci) {
      vector<int>& src = corpuse[ci];
      Lattice lat(src.size());
      for (unsigned i = 0; i < src.size(); ++i)
        lat[i].push_back(LatticeArc(src[i], 0.0, 1));
      cerr << TD::GetString(src) << endl;
      hg.clear();
      parser.Parse(lat, &hg);  // exhaustive parse
      DecrementDerivation(hg, derivs[ci], &lm, &rng);
      for (unsigned i = 0; i < hg.edges_.size(); ++i) {
        TRule& r = *hg.edges_[i].rule_;
        if (r.lhs_ == kX)
          hg.edges_[i].edge_prob_ = lm.Prob(r);
      }
      vector<unsigned> d;
      SampleDerivation(hg, &rng, &d, &lm);
      derivs[ci] = d;
      IncrementDerivation(hg, derivs[ci], &lm, &rng);
      if (tofreelist.size() > 100000) {
        cerr << "Freeing ... ";
        for (unsigned i = 0; i < tofreelist.size(); ++i)
          delete tofreelist[i];
        tofreelist.clear();
        cerr << "Freed.\n";
      }
    }
    cerr << "LLH=" << lm.Likelihood() << endl;
  }
  return 0;
}

