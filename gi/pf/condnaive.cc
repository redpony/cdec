#include <iostream>
#include <tr1/memory>
#include <queue>

#include <boost/multi_array.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "base_distributions.h"
#include "monotonic_pseg.h"
#include "conditional_pseg.h"
#include "trule.h"
#include "tdict.h"
#include "filelib.h"
#include "dict.h"
#include "sampler.h"
#include "ccrp_nt.h"
#include "corpus.h"

using namespace std;
using namespace std::tr1;
namespace po = boost::program_options;

static unsigned kMAX_SRC_PHRASE;
static unsigned kMAX_TRG_PHRASE;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,s",po::value<unsigned>()->default_value(1000),"Number of samples")
        ("input,i",po::value<string>(),"Read parallel data from")
        ("max_src_phrase",po::value<unsigned>()->default_value(4),"Maximum length of source language phrases")
        ("max_trg_phrase",po::value<unsigned>()->default_value(4),"Maximum length of target language phrases")
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

shared_ptr<MT19937> prng;

struct ModelAndData {
  explicit ModelAndData(ConditionalParallelSegementationModel<PhraseConditionalBase>& m, const vector<vector<int> >& ce, const vector<vector<int> >& cf, const set<int>& ve, const set<int>& vf) :
     model(m),
     rng(&*prng),
     corpuse(ce),
     corpusf(cf),
     vocabe(ve),
     vocabf(vf),
     mh_samples(),
     mh_rejects(),
     kX(-TD::Convert("X")),
     derivations(corpuse.size()) {}

  void ResampleHyperparameters() {
  }

  void InstantiateRule(const pair<short,short>& from,
                       const pair<short,short>& to,
                       const vector<int>& sentf,
                       const vector<int>& sente,
                       TRule* rule) const {
    rule->f_.clear();
    rule->e_.clear();
    rule->lhs_ = kX;
    for (short i = from.first; i < to.first; ++i)
      rule->f_.push_back(sentf[i]);
    for (short i = from.second; i < to.second; ++i)
      rule->e_.push_back(sente[i]);
  }

  void DecrementDerivation(const vector<pair<short,short> >& d, const vector<int>& sentf, const vector<int>& sente) {
    if (d.size() < 2) return;
    TRule x;
    for (int i = 1; i < d.size(); ++i) {
      InstantiateRule(d[i], d[i-1], sentf, sente, &x);
      model.DecrementRule(x);
      model.DecrementAlign(x.f_.size());
    }
  }

  void PrintDerivation(const vector<pair<short,short> >& d, const vector<int>& sentf, const vector<int>& sente) {
    if (d.size() < 2) return;
    TRule x;
    for (int i = 1; i < d.size(); ++i) {
      InstantiateRule(d[i], d[i-1], sentf, sente, &x);
      cerr << i << '/' << (d.size() - 1) << ": " << x << endl;
    }
  }

  void IncrementDerivation(const vector<pair<short,short> >& d, const vector<int>& sentf, const vector<int>& sente) {
    if (d.size() < 2) return;
    TRule x;
    for (int i = 1; i < d.size(); ++i) {
      InstantiateRule(d[i], d[i-1], sentf, sente, &x);
      model.IncrementRule(x);
      model.IncrementAlign(x.f_.size());
    }
  }

  prob_t Likelihood() const {
    return model.Likelihood();
  }

  prob_t DerivationProposalProbability(const vector<pair<short,short> >& d, const vector<int>& sentf, const vector<int>& sente) const {
    prob_t p = prob_t::One();
    TRule x;
    for (int i = 1; i < d.size(); ++i) {
      InstantiateRule(d[i], d[i-1], sentf, sente, &x);
      p *= model.RuleProbability(x);
      p *= model.AlignProbability(x.f_.size());
    }
    return p;
  }

  void Sample();

  ConditionalParallelSegementationModel<PhraseConditionalBase>& model;
  MT19937* rng;
  const vector<vector<int> >& corpuse, corpusf;
  const set<int>& vocabe, vocabf;
  unsigned mh_samples, mh_rejects;
  const int kX;
  vector<vector<pair<short, short> > > derivations;
};

void ModelAndData::Sample() {
  unsigned MAXK = kMAX_SRC_PHRASE;
  unsigned MAXL = kMAX_TRG_PHRASE;
  TRule x;
  x.lhs_ = -TD::Convert("X");

  for (int samples = 0; samples < 1000; ++samples) {
    if (samples % 1 == 0 && samples > 0) {
      //ResampleHyperparameters();
      cerr << " [" << samples << " LLH=" << log(Likelihood()) << " MH=" << ((double)mh_rejects / mh_samples) << "]\n";
      for (int i = 0; i < 10; ++i) {
        cerr << "SENTENCE: " << TD::GetString(corpusf[i]) << " ||| " << TD::GetString(corpuse[i]) << endl;
        PrintDerivation(derivations[i], corpusf[i], corpuse[i]);
      }
      static TRule xx("[X] ||| w n ||| s h ||| X=0");
      const CCRP_NoTable<TRule>& dcrp = model.tmodel.r.find(xx.f_)->second;
      for (CCRP_NoTable<TRule>::const_iterator it = dcrp.begin(); it != dcrp.end(); ++it) {
        cerr << "\t" << it->second << "\t" << it->first << endl;
      }
    }
    cerr << '.' << flush;
    for (int s = 0; s < corpuse.size(); ++s) {
      const vector<int>& sentf = corpusf[s];
      const vector<int>& sente = corpuse[s];
//      cerr << "  CUSTOMERS: " << rules.num_customers() << endl;
//      cerr << "SENTENCE: " << TD::GetString(sentf) << " ||| " << TD::GetString(sente) << endl;

      vector<pair<short, short> >& deriv = derivations[s];
      const prob_t p_cur = Likelihood();
      DecrementDerivation(deriv, sentf, sente);

      boost::multi_array<prob_t, 2> a(boost::extents[sentf.size() + 1][sente.size() + 1]);
      boost::multi_array<prob_t, 4> trans(boost::extents[sentf.size() + 1][sente.size() + 1][MAXK][MAXL]);
      a[0][0] = prob_t::One();
      for (int i = 0; i < sentf.size(); ++i) {
        for (int j = 0; j < sente.size(); ++j) {
          const prob_t src_a = a[i][j];
          x.f_.clear();
          for (int k = 1; k <= MAXK; ++k) {
            if (i + k > sentf.size()) break;
            x.f_.push_back(sentf[i + k - 1]);
            x.e_.clear();
            const prob_t p_span = model.AlignProbability(k);  // prob of consuming this much source
            for (int l = 1; l <= MAXL; ++l) {
              if (j + l > sente.size()) break;
              x.e_.push_back(sente[j + l - 1]);
              trans[i][j][k - 1][l - 1] = model.RuleProbability(x) * p_span;
              a[i + k][j + l] += src_a * trans[i][j][k - 1][l - 1];
            }
          }
        }
      }
//      cerr << "Inside: " << log(a[sentf.size()][sente.size()]) << endl;
      const prob_t q_cur = DerivationProposalProbability(deriv, sentf, sente);

      vector<pair<short,short> > newderiv;
      int cur_i = sentf.size();
      int cur_j = sente.size();
      while(cur_i > 0 && cur_j > 0) {
        newderiv.push_back(pair<short,short>(cur_i, cur_j));
//        cerr << "NODE: (" << cur_i << "," << cur_j << ")\n";
        SampleSet<prob_t> ss;
        vector<pair<short,short> > nexts;
        for (int k = 1; k <= MAXK; ++k) {
          const int hyp_i = cur_i - k;
          if (hyp_i < 0) break;
          for (int l = 1; l <= MAXL; ++l) {
            const int hyp_j = cur_j - l;
            if (hyp_j < 0) break;
            const prob_t& inside = a[hyp_i][hyp_j];
            if (inside == prob_t::Zero()) continue;
            const prob_t& transp = trans[hyp_i][hyp_j][k - 1][l - 1];
            if (transp == prob_t::Zero()) continue;
            const prob_t p = inside * transp;
            ss.add(p);
            nexts.push_back(pair<short,short>(hyp_i, hyp_j));
//            cerr << "    (" << hyp_i << "," << hyp_j << ")  <--- " << log(p) << endl;
          }
        }
//        cerr << "  sample set has " << nexts.size() << " elements.\n";
        const int selected = rng->SelectSample(ss);
        cur_i = nexts[selected].first;
        cur_j = nexts[selected].second;
      }
      newderiv.push_back(pair<short,short>(0,0));
      const prob_t q_new = DerivationProposalProbability(newderiv, sentf, sente);
      IncrementDerivation(newderiv, sentf, sente);
//      cerr << "SANITY: " << q_new << "  " <<log(DerivationProposalProbability(newderiv, sentf, sente)) << endl;
      if (deriv.empty()) { deriv = newderiv; continue; }
      ++mh_samples;

      if (deriv != newderiv) {
        const prob_t p_new = Likelihood();
//        cerr << "p_cur=" << log(p_cur) << "\t p_new=" << log(p_new) << endl;
//        cerr << "q_cur=" << log(q_cur) << "\t q_new=" << log(q_new) << endl;
        if (!rng->AcceptMetropolisHastings(p_new, p_cur, q_new, q_cur)) {
          ++mh_rejects;
          DecrementDerivation(newderiv, sentf, sente);
          IncrementDerivation(deriv, sentf, sente);
        } else {
//          cerr << "  ACCEPT\n";
          deriv = newderiv;
        }
      }
    }
  }
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
  if (conf.count("random_seed"))
    prng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    prng.reset(new MT19937);
//  MT19937& rng = *prng;

  vector<vector<int> > corpuse, corpusf;
  set<int> vocabe, vocabf;
  corpus::ReadParallelCorpus(conf["input"].as<string>(), &corpusf, &corpuse, &vocabf, &vocabe);
  cerr << "f-Corpus size: " << corpusf.size() << " sentences\n";
  cerr << "f-Vocabulary size: " << vocabf.size() << " types\n";
  cerr << "f-Corpus size: " << corpuse.size() << " sentences\n";
  cerr << "f-Vocabulary size: " << vocabe.size() << " types\n";
  assert(corpusf.size() == corpuse.size());

  Model1 m1(conf["model1"].as<string>());

  PhraseConditionalBase pcb0(m1, conf["model1_interpolation_weight"].as<double>(), vocabe.size());
  ConditionalParallelSegementationModel<PhraseConditionalBase> x(pcb0);  

  ModelAndData posterior(x, corpuse, corpusf, vocabe, vocabf);
  posterior.Sample();

  TRule r1("[X] ||| x ||| l e ||| X=0");
  TRule r2("[X] ||| A ||| a d ||| X=0");
  TRule r3("[X] ||| n ||| e r ||| X=0");
  TRule r4("[X] ||| x A n ||| b l a g ||| X=0");

  PhraseConditionalUninformativeBase u0(vocabe.size());

  cerr << (pcb0(r1)*pcb0(r2)*pcb0(r3)) << endl;
  cerr << (u0(r4)) << endl;

  return 0;
}

