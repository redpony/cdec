#include <iostream>
#include <queue>

#include <boost/functional.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "inside_outside.h"
#include "hg.h"
#include "hg_io.h"
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

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,s",po::value<unsigned>()->default_value(1000),"Number of samples")
        ("input,i",po::value<string>(),"Read parallel data from")
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
                    vector<Lattice>* e,
                    set<WordID>* vocab_e) {
  e->clear();
  vocab_e->clear();
  ReadFile rf(filename);
  istream* in = rf.stream();
  assert(*in);
  string line;
  unsigned toks = 0;
  while(*in) {
    getline(*in, line);
    if (line.empty() && !*in) break;
    e->push_back(Lattice());
    Lattice& le = e->back();
    LatticeTools::ConvertTextOrPLF(line, & le);
    for (unsigned i = 0; i < le.size(); ++i)
      for (unsigned j = 0; j < le[i].size(); ++j)
        vocab_e->insert(le[i][j].label);
    toks += le.size();
  }
  return toks;
}

struct BaseModel {
  explicit BaseModel(unsigned tc) :
      unif(1.0 / tc), p(prob_t::One()) {}
  prob_t prob(const TRule& r) const {
    return unif;
  }
  void increment(const TRule& r, MT19937* rng) {
    p *= prob(r);
  }
  void decrement(const TRule& r, MT19937* rng) {
    p /= prob(r);
  }
  prob_t Likelihood() const {
    return p;
  }
  const prob_t unif;
  prob_t p;
};

struct UnigramModel {
  explicit UnigramModel(unsigned tc) : base(tc), crp(1,1,1,1), glue(1,1,1,1) {}
  BaseModel base;
  CCRP<TRule> crp;
  CCRP<TRule> glue;

  prob_t Prob(const TRule& r) const {
    if (r.Arity() != 0) {
      return glue.prob(r, prob_t(0.5));
    }
    return crp.prob(r, base.prob(r));
  }

  int Increment(const TRule& r, MT19937* rng) {
    if (r.Arity() != 0) {
      glue.increment(r, 0.5, rng);
      return 0;
    } else {
      if (crp.increment(r, base.prob(r), rng)) {
        base.increment(r, rng);
        return 1;
      }
      return 0;
    }
  }

  int Decrement(const TRule& r, MT19937* rng) {
    if (r.Arity() != 0) {
      glue.decrement(r, rng);
      return 0;
    } else {
      if (crp.decrement(r, rng)) {
        base.decrement(r, rng);
        return -1;
      }
      return 0;
    }
  }

  prob_t Likelihood() const {
    prob_t p;
    p.logeq(crp.log_crp_prob() + glue.log_crp_prob());
    p *= base.Likelihood();
    return p;
  }

  void ResampleHyperparameters(MT19937* rng) {
    crp.resample_hyperparameters(rng);
    glue.resample_hyperparameters(rng);
    cerr << " d=" << crp.discount() << ", s=" << crp.strength() << "\t STOP d=" << glue.discount() << ", s=" << glue.strength() << endl;
  }
};

UnigramModel* plm;

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
//  for (unsigned i = 0; i < sampled_deriv->size(); ++i) {
//    cerr << *hg.edges_[(*sampled_deriv)[i]].rule_ << endl;
//  }
}

void IncrementDerivation(const Hypergraph& hg, const vector<unsigned>& d, UnigramModel* plm, MT19937* rng) {
  for (unsigned i = 0; i < d.size(); ++i)
    plm->Increment(*hg.edges_[d[i]].rule_, rng);
}

void DecrementDerivation(const Hypergraph& hg, const vector<unsigned>& d, UnigramModel* plm, MT19937* rng) {
  for (unsigned i = 0; i < d.size(); ++i)
    plm->Decrement(*hg.edges_[d[i]].rule_, rng);
}

prob_t TotalProb(const Hypergraph& hg) {
  return Inside<prob_t, EdgeProb>(hg);
}

void IncrementLatticePath(const Hypergraph& hg, const vector<unsigned>& d, Lattice* pl) {
  Lattice& lat = *pl;
  for (int i = 0; i < d.size(); ++i) {
    const Hypergraph::Edge& edge = hg.edges_[d[i]];
    if (edge.rule_->Arity() != 0) continue;
    WordID sym = edge.rule_->e_[0];
    vector<LatticeArc>& las = lat[edge.i_];
    int dist = edge.j_ - edge.i_;
    assert(dist > 0);
    for (int j = 0; j < las.size(); ++j) {
      if (las[j].dist2next == dist &&
          las[j].label == sym) {
        las[j].cost += 1;
      }
    }
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;

  InitCommandLine(argc, argv, &conf);
  vector<GrammarPtr> grammars(2);
  grammars[0].reset(new GlueGrammar("S","X"));
  const unsigned samples = conf["samples"].as<unsigned>();

  if (conf.count("random_seed"))
    prng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    prng.reset(new MT19937);
  MT19937& rng = *prng;
  vector<Lattice> corpuse;
  set<WordID> vocabe;
  cerr << "Reading corpus...\n";
  const unsigned toks = ReadCorpus(conf["input"].as<string>(), &corpuse, &vocabe);
  cerr << "E-corpus size: " << corpuse.size() << " lattices\t (" << vocabe.size() << " word types)\n";
  UnigramModel lm(vocabe.size());
  vector<Hypergraph> hgs(corpuse.size());
  vector<vector<unsigned> > derivs(corpuse.size());
  for (int i = 0; i < corpuse.size(); ++i) {
    grammars[1].reset(new PassThroughGrammar(corpuse[i], "X"));
    ExhaustiveBottomUpParser parser("S", grammars);
    bool res = parser.Parse(corpuse[i], &hgs[i]);  // exhaustive parse
    assert(res);
  }

  double csamples = 0;
  for (int SS=0; SS < samples; ++SS) {
    const bool is_last = ((samples - 1) == SS);
    prob_t dlh = prob_t::One();
    bool record_sample = (SS > (samples * 1 / 3) && (SS % 5 == 3));
    if (record_sample) csamples++;
    for (int ci = 0; ci < corpuse.size(); ++ci) {
      Lattice& lat = corpuse[ci];
      Hypergraph& hg = hgs[ci];
      vector<unsigned>& d = derivs[ci];
      if (!is_last) DecrementDerivation(hg, d, &lm, &rng);
      for (unsigned i = 0; i < hg.edges_.size(); ++i) {
        TRule& r = *hg.edges_[i].rule_;
        if (r.Arity() != 0)
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
      if (record_sample) IncrementLatticePath(hg, derivs[ci], &lat);
    }
    double llh = log(lm.Likelihood());
    cerr << "LLH=" << llh << "\tENTROPY=" << (-llh / log(2) / toks) << "\tPPL=" << pow(2, -llh / log(2) / toks) << endl;
    if (SS % 10 == 9) lm.ResampleHyperparameters(&rng);
    if (is_last) {
      double z = log(dlh);
      cerr << "TOTAL_PROB=" << z << "\tENTROPY=" << (-z / log(2) / toks) << "\tPPL=" << pow(2, -z / log(2) / toks) << endl;
    }
  }
  cerr << lm.crp << endl;
  cerr << lm.glue << endl;
  for (int i = 0; i < corpuse.size(); ++i) {
    for (int j = 0; j < corpuse[i].size(); ++j)
      for (int k = 0; k < corpuse[i][j].size(); ++k) {
        corpuse[i][j][k].cost /= csamples;
        corpuse[i][j][k].cost += 1e-3;
        corpuse[i][j][k].cost = log(corpuse[i][j][k].cost);
      }
    cout << HypergraphIO::AsPLF(corpuse[i]) << endl;
  }
  return 0;
}

