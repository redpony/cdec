#include <iostream>
#include <tr1/memory>
#include <queue>

#include <boost/functional.hpp>
#include <boost/multi_array.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "viterbi.h"
#include "hg.h"
#include "trule.h"
#include "tdict.h"
#include "filelib.h"
#include "dict.h"
#include "sampler.h"
#include "ccrp_nt.h"
#include "cfg_wfst_composer.h"

using namespace std;
using namespace tr1;
namespace po = boost::program_options;

static unsigned kMAX_SRC_PHRASE;
static unsigned kMAX_TRG_PHRASE;
struct FSTState;

double log_poisson(unsigned x, const double& lambda) {
  assert(lambda > 0.0);
  return log(lambda) * x - lgamma(x + 1) - lambda;
}

struct ConditionalBase {
  explicit ConditionalBase(const double m1mixture, const unsigned vocab_e_size, const string& model1fname) :
      kM1MIXTURE(m1mixture),
      kUNIFORM_MIXTURE(1.0 - m1mixture),
      kUNIFORM_TARGET(1.0 / vocab_e_size),
      kNULL(TD::Convert("<eps>")) {
    assert(m1mixture >= 0.0 && m1mixture <= 1.0);
    assert(vocab_e_size > 0);
    LoadModel1(model1fname);
  }

  void LoadModel1(const string& fname) {
    cerr << "Loading Model 1 parameters from " << fname << " ..." << endl;
    ReadFile rf(fname);
    istream& in = *rf.stream();
    string line;
    unsigned lc = 0;
    while(getline(in, line)) {
      ++lc;
      int cur = 0;
      int start = 0;
      while(cur < line.size() && line[cur] != ' ') { ++cur; }
      assert(cur != line.size());
      line[cur] = 0;
      const WordID src = TD::Convert(&line[0]);
      ++cur;
      start = cur;
      while(cur < line.size() && line[cur] != ' ') { ++cur; }
      assert(cur != line.size());
      line[cur] = 0;
      WordID trg = TD::Convert(&line[start]);
      const double logprob = strtod(&line[cur + 1], NULL);
      if (src >= ttable.size()) ttable.resize(src + 1);
      ttable[src][trg].logeq(logprob);
    }
    cerr << "  read " << lc << " parameters.\n";
  }

  // return logp0 of rule.e_ | rule.f_
  prob_t operator()(const TRule& rule) const {
    const int flen = rule.f_.size();
    const int elen = rule.e_.size();
    prob_t uniform_src_alignment; uniform_src_alignment.logeq(-log(flen + 1));
    prob_t p;
    p.logeq(log_poisson(elen, flen + 0.01));       // elen | flen          ~Pois(flen + 0.01)
    for (int i = 0; i < elen; ++i) {               // for each position i in e-RHS
      const WordID trg = rule.e_[i];
      prob_t tp = prob_t::Zero();
      for (int j = -1; j < flen; ++j) {
        const WordID src = j < 0 ? kNULL : rule.f_[j];
        const map<WordID, prob_t>::const_iterator it = ttable[src].find(trg);
        if (it != ttable[src].end()) {
          tp += kM1MIXTURE * it->second;
        }
        tp += kUNIFORM_MIXTURE * kUNIFORM_TARGET;
      }
      tp *= uniform_src_alignment;                 //     draw a_i         ~uniform
      p *= tp;                                     //     draw e_i         ~Model1(f_a_i) / uniform
    }
    return p;
  }

  const prob_t kM1MIXTURE;  // Model 1 mixture component
  const prob_t kUNIFORM_MIXTURE; // uniform mixture component
  const prob_t kUNIFORM_TARGET;
  const WordID kNULL;
  vector<map<WordID, prob_t> > ttable;
};

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,s",po::value<unsigned>()->default_value(1000),"Number of samples")
        ("input,i",po::value<string>(),"Read parallel data from")
        ("max_src_phrase",po::value<unsigned>()->default_value(3),"Maximum length of source language phrases")
        ("max_trg_phrase",po::value<unsigned>()->default_value(3),"Maximum length of target language phrases")
        ("model1,m",po::value<string>(),"Model 1 parameters (used in base distribution)")
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
                vector<vector<int> >* e,
                set<int>* vocab_f,
                set<int>* vocab_e) {
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

struct UniphraseLM {
  UniphraseLM(const vector<vector<int> >& corpus,
              const set<int>& vocab,
              const po::variables_map& conf) :
    phrases_(1,1),
    gen_(1,1),
    corpus_(corpus),
    uniform_word_(1.0 / vocab.size()),
    gen_p0_(0.5),
    p_end_(0.5),
    use_poisson_(conf.count("poisson_length") > 0) {}

  void ResampleHyperparameters(MT19937* rng) {
    phrases_.resample_hyperparameters(rng);
    gen_.resample_hyperparameters(rng);
    cerr << " " << phrases_.alpha();
  }

  CCRP_NoTable<vector<int> > phrases_;
  CCRP_NoTable<bool> gen_;
  vector<vector<bool> > z_;   // z_[i] is there a phrase boundary after the ith word
  const vector<vector<int> >& corpus_;
  const double uniform_word_;
  const double gen_p0_;
  const double p_end_; // in base length distribution, p of the end of a phrase
  const bool use_poisson_;
};

struct Reachability {
  boost::multi_array<bool, 4> edges;  // edges[src_covered][trg_covered][x][trg_delta] is this edge worth exploring?
  boost::multi_array<short, 2> max_src_delta; // msd[src_covered][trg_covered] -- the largest src delta that's valid

  Reachability(int srclen, int trglen, int src_max_phrase_len, int trg_max_phrase_len) :
      edges(boost::extents[srclen][trglen][src_max_phrase_len+1][trg_max_phrase_len+1]),
      max_src_delta(boost::extents[srclen][trglen]) {
    ComputeReachability(srclen, trglen, src_max_phrase_len, trg_max_phrase_len);
  }

 private:
  struct SState {
    SState() : prev_src_covered(), prev_trg_covered() {}
    SState(int i, int j) : prev_src_covered(i), prev_trg_covered(j) {}
    int prev_src_covered;
    int prev_trg_covered;
  };

  struct NState {
    NState() : next_src_covered(), next_trg_covered() {}
    NState(int i, int j) : next_src_covered(i), next_trg_covered(j) {}
    int next_src_covered;
    int next_trg_covered;
  };

  void ComputeReachability(int srclen, int trglen, int src_max_phrase_len, int trg_max_phrase_len) {
    typedef boost::multi_array<vector<SState>, 2> array_type;
    array_type a(boost::extents[srclen + 1][trglen + 1]);
    a[0][0].push_back(SState());
    for (int i = 0; i < srclen; ++i) {
      for (int j = 0; j < trglen; ++j) {
        if (a[i][j].size() == 0) continue;
        const SState prev(i,j);
        for (int k = 1; k <= src_max_phrase_len; ++k) {
          if ((i + k) > srclen) continue;
          for (int l = 1; l <= trg_max_phrase_len; ++l) {
            if ((j + l) > trglen) continue;
            a[i + k][j + l].push_back(prev);
          }
        }
      }
    }
    a[0][0].clear();
    cerr << "Final cell contains " << a[srclen][trglen].size() << " back pointers\n";
    assert(a[srclen][trglen].size() > 0);

    typedef boost::multi_array<bool, 2> rarray_type;
    rarray_type r(boost::extents[srclen + 1][trglen + 1]);
//    typedef boost::multi_array<vector<NState>, 2> narray_type;
//    narray_type b(boost::extents[srclen + 1][trglen + 1]);
    r[srclen][trglen] = true;
    for (int i = srclen; i >= 0; --i) {
      for (int j = trglen; j >= 0; --j) {
        vector<SState>& prevs = a[i][j];
        if (!r[i][j]) { prevs.clear(); }
//        const NState nstate(i,j);
        for (int k = 0; k < prevs.size(); ++k) {
          r[prevs[k].prev_src_covered][prevs[k].prev_trg_covered] = true;
          int src_delta = i - prevs[k].prev_src_covered;
          edges[prevs[k].prev_src_covered][prevs[k].prev_trg_covered][src_delta][j - prevs[k].prev_trg_covered] = true;
          short &msd = max_src_delta[prevs[k].prev_src_covered][prevs[k].prev_trg_covered];
          if (src_delta > msd) msd = src_delta;
//          b[prevs[k].prev_src_covered][prevs[k].prev_trg_covered].push_back(nstate);
        }
      }
    }
    assert(!edges[0][0][1][0]);
    assert(!edges[0][0][0][1]);
    assert(!edges[0][0][0][0]);
    cerr << "  MAX SRC DELTA[0][0] = " << max_src_delta[0][0] << endl;
    assert(max_src_delta[0][0] > 0);
    //cerr << "First cell contains " << b[0][0].size() << " forward pointers\n";
    //for (int i = 0; i < b[0][0].size(); ++i) {
    //  cerr << "  -> (" << b[0][0][i].next_src_covered << "," << b[0][0][i].next_trg_covered << ")\n";
    //}
  }
};

ostream& operator<<(ostream& os, const FSTState& q);
struct FSTState {
  explicit FSTState(int src_size) :
      trg_covered_(),
      src_covered_(),
      src_coverage_(src_size) {}

  FSTState(short trg_covered, short src_covered, const vector<bool>& src_coverage, const vector<short>& src_prefix) :
      trg_covered_(trg_covered),
      src_covered_(src_covered),
      src_coverage_(src_coverage),
      src_prefix_(src_prefix) {
    if (src_coverage_.size() == src_covered) {
      assert(src_prefix.size() == 0);
    }
  }

  // if we extend by the word at src_position, what are
  // the next states that are reachable and lie on a valid
  // path to the final state?
  vector<FSTState> Extensions(int src_position, int src_len, int trg_len, const Reachability& r) const {
    assert(src_position < src_coverage_.size());
    if (src_coverage_[src_position]) {
      cerr << "Trying to extend " << *this << " with position " << src_position << endl;
      abort();
    }
    vector<bool> ncvg = src_coverage_;
    ncvg[src_position] = true;

    vector<FSTState> res;
    const int trg_remaining = trg_len - trg_covered_;
    if (trg_remaining <= 0) {
      cerr << "Target appears to have been covered: " << *this << " (trg_len=" << trg_len << ",trg_covered=" << trg_covered_ << ")" << endl;
      abort();
    }
    const int src_remaining = src_len - src_covered_;
    if (src_remaining <= 0) {
      cerr << "Source appears to have been covered: " << *this << endl;
      abort();
    }

    for (int tc = 1; tc <= kMAX_TRG_PHRASE; ++tc) {
      if (r.edges[src_covered_][trg_covered_][src_prefix_.size() + 1][tc]) {
        int nc = src_prefix_.size() + 1 + src_covered_;
        res.push_back(FSTState(trg_covered_ + tc, nc, ncvg, vector<short>()));
      }
    }

    if ((src_prefix_.size() + 1) < r.max_src_delta[src_covered_][trg_covered_]) {
      vector<short> nsp = src_prefix_;
      nsp.push_back(src_position);
      res.push_back(FSTState(trg_covered_, src_covered_, ncvg, nsp));
    }

    if (res.size() == 0) {
      cerr << *this << " can't be extended!\n";
      abort();
    }
    return res;
  }

  short trg_covered_, src_covered_;
  vector<bool> src_coverage_;
  vector<short> src_prefix_;
};
bool operator<(const FSTState& q, const FSTState& r) {
  if (q.trg_covered_ != r.trg_covered_) return q.trg_covered_ < r.trg_covered_;
  if (q.src_covered_!= r.src_covered_) return q.src_covered_ < r.src_covered_;
  if (q.src_coverage_ != r.src_coverage_) return q.src_coverage_ < r.src_coverage_;
  return q.src_prefix_ < r.src_prefix_;
}

ostream& operator<<(ostream& os, const FSTState& q) {
  os << "[" << q.trg_covered_ << " : ";
  for (int i = 0; i < q.src_coverage_.size(); ++i)
    os << q.src_coverage_[i];
  os << " : <";
  for (int i = 0; i < q.src_prefix_.size(); ++i) {
    if (i != 0) os << ' ';
    os << q.src_prefix_[i];
  }
  return os << ">]";
}

struct MyModel {
  MyModel(ConditionalBase& rcp0) : rp0(rcp0) {}
  typedef unordered_map<vector<WordID>, CCRP_NoTable<TRule>, boost::hash<vector<WordID> > > SrcToRuleCRPMap;

  void DecrementRule(const TRule& rule) {
    SrcToRuleCRPMap::iterator it = rules.find(rule.f_);
    assert(it != rules.end());
    it->second.decrement(rule);
    if (it->second.num_customers() == 0) rules.erase(it);
  }

  void IncrementRule(const TRule& rule) {
    SrcToRuleCRPMap::iterator it = rules.find(rule.f_);
    if (it == rules.end()) {
      CCRP_NoTable<TRule> crp(1,1);
      it = rules.insert(make_pair(rule.f_, crp)).first;
    }
    it->second.increment(rule);
  }

  // conditioned on rule.f_
  prob_t RuleConditionalProbability(const TRule& rule) const {
    const prob_t base = rp0(rule);
    SrcToRuleCRPMap::const_iterator it = rules.find(rule.f_);
    if (it == rules.end()) {
      return base;
    } else {
      const double lp = it->second.logprob(rule, log(base));
      prob_t q; q.logeq(lp);
      return q;
    }
  }

  const ConditionalBase& rp0;
  SrcToRuleCRPMap rules;
};

struct MyFST : public WFST {
  MyFST(const vector<WordID>& ssrc, const vector<WordID>& strg, MyModel* m) :
      src(ssrc), trg(strg),
      r(src.size(),trg.size(),kMAX_SRC_PHRASE, kMAX_TRG_PHRASE),
      model(m) {
    FSTState in(src.size());
    cerr << " INIT: " << in << endl;
    init = GetNode(in);
    for (int i = 0; i < in.src_coverage_.size(); ++i) in.src_coverage_[i] = true;
    in.src_covered_ = src.size();
    in.trg_covered_ = trg.size();
    cerr << "FINAL: " << in << endl;
    final = GetNode(in);
  }
  virtual const WFSTNode* Final() const;
  virtual const WFSTNode* Initial() const;

  const WFSTNode* GetNode(const FSTState& q);
  map<FSTState, boost::shared_ptr<WFSTNode> > m;
  const vector<WordID>& src;
  const vector<WordID>& trg;
  Reachability r;
  const WFSTNode* init;
  const WFSTNode* final;
  MyModel* model;
};

struct MyNode : public WFSTNode {
  MyNode(const FSTState& q, MyFST* fst) : state(q), container(fst) {}
  virtual vector<pair<const WFSTNode*, TRulePtr> > ExtendInput(unsigned srcindex) const;
  const FSTState state;
  mutable MyFST* container;
};

vector<pair<const WFSTNode*, TRulePtr> > MyNode::ExtendInput(unsigned srcindex) const {
  cerr << "EXTEND " << state << " with " << srcindex << endl;
  vector<FSTState> ext = state.Extensions(srcindex, container->src.size(), container->trg.size(), container->r);
  vector<pair<const WFSTNode*,TRulePtr> > res(ext.size());
  for (unsigned i = 0; i < ext.size(); ++i) {
    res[i].first = container->GetNode(ext[i]);
    if (ext[i].src_prefix_.size() == 0) {
      const unsigned trg_from = state.trg_covered_;
      const unsigned trg_to = ext[i].trg_covered_;
      const unsigned prev_prfx_size = state.src_prefix_.size();
      res[i].second.reset(new TRule);
      res[i].second->lhs_ = -TD::Convert("X");
      vector<WordID>& src = res[i].second->f_;
      vector<WordID>& trg = res[i].second->e_;
      src.resize(prev_prfx_size + 1);
      for (unsigned j = 0; j < prev_prfx_size; ++j)
        src[j] = container->src[state.src_prefix_[j]];
      src[prev_prfx_size] = container->src[srcindex];
      for (unsigned j = trg_from; j < trg_to; ++j)
        trg.push_back(container->trg[j]);
      res[i].second->scores_.set_value(FD::Convert("Proposal"), log(container->model->RuleConditionalProbability(*res[i].second)));
    }
  }
  return res;
}

const WFSTNode* MyFST::GetNode(const FSTState& q) {
  boost::shared_ptr<WFSTNode>& res = m[q];
  if (!res) {
    res.reset(new MyNode(q, this));
  }
  return &*res;
}

const WFSTNode* MyFST::Final() const {
  return final;
}

const WFSTNode* MyFST::Initial() const {
  return init;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  kMAX_TRG_PHRASE = conf["max_trg_phrase"].as<unsigned>();
  kMAX_SRC_PHRASE = conf["max_src_phrase"].as<unsigned>();

  if (!conf.count("model1")) {
    cerr << argv[0] << "Please use --model1 to specify model 1 parameters\n";
    return 1;
  }
  shared_ptr<MT19937> prng;
  if (conf.count("random_seed"))
    prng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    prng.reset(new MT19937);
  MT19937& rng = *prng;

  vector<vector<int> > corpuse, corpusf;
  set<int> vocabe, vocabf;
  ReadParallelCorpus(conf["input"].as<string>(), &corpusf, &corpuse, &vocabf, &vocabe);
  cerr << "f-Corpus size: " << corpusf.size() << " sentences\n";
  cerr << "f-Vocabulary size: " << vocabf.size() << " types\n";
  cerr << "f-Corpus size: " << corpuse.size() << " sentences\n";
  cerr << "f-Vocabulary size: " << vocabe.size() << " types\n";
  assert(corpusf.size() == corpuse.size());

  ConditionalBase lp0(conf["model1_interpolation_weight"].as<double>(),
                      vocabe.size(),
                      conf["model1"].as<string>());
  MyModel m(lp0);

  TRule x("[X] ||| kAnwntR myN ||| at the convent ||| 0");
  m.IncrementRule(x);
  TRule y("[X] ||| nY dyN ||| gave ||| 0");
  m.IncrementRule(y);


  MyFST fst(corpusf[0], corpuse[0], &m);
  ifstream in("./kimura.g");
  assert(in);
  CFG_WFSTComposer comp(fst);
  Hypergraph hg;
  bool succeed = comp.Compose(&in, &hg);
  hg.PrintGraphviz();
  if (succeed) { cerr << "SUCCESS.\n"; } else { cerr << "FAILURE REPORTED.\n"; }

#if 0
  ifstream in2("./amnabooks.g");
  assert(in2);
  MyFST fst2(corpusf[1], corpuse[1], &m);
  CFG_WFSTComposer comp2(fst2);
  Hypergraph hg2;
  bool succeed2 = comp2.Compose(&in2, &hg2);
  if (succeed2) { cerr << "SUCCESS.\n"; } else { cerr << "FAILURE REPORTED.\n"; }
#endif

  SparseVector<double> w; w.set_value(FD::Convert("Proposal"), 1.0);
  hg.Reweight(w);
  cerr << ViterbiFTree(hg) << endl;
  return 0;
}

