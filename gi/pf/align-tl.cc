#include <iostream>
#include <tr1/memory>
#include <queue>

#include <boost/multi_array.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "backward.h"
#include "array2d.h"
#include "base_distributions.h"
#include "monotonic_pseg.h"
#include "conditional_pseg.h"
#include "trule.h"
#include "tdict.h"
#include "stringlib.h"
#include "filelib.h"
#include "dict.h"
#include "sampler.h"
#include "mfcr.h"
#include "corpus.h"
#include "ngram_base.h"
#include "transliterations.h"

using namespace std;
using namespace tr1;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,s",po::value<unsigned>()->default_value(1000),"Number of samples")
        ("input,i",po::value<string>(),"Read parallel data from")
        ("s2t", po::value<string>(), "character level source-to-target prior transliteration probabilities")
        ("t2s", po::value<string>(), "character level target-to-source prior transliteration probabilities")
        ("max_src_chunk", po::value<unsigned>()->default_value(4), "Maximum size of translitered chunk in source")
        ("max_trg_chunk", po::value<unsigned>()->default_value(4), "Maximum size of translitered chunk in target")
        ("expected_src_to_trg_ratio", po::value<double>()->default_value(1.0), "If a word is transliterated, what is the expected length ratio from source to target?")
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

boost::shared_ptr<MT19937> prng;

struct LexicalAlignment {
  unsigned char src_index;
  bool is_transliteration;
  vector<pair<short, short> > derivation;
};

struct AlignedSentencePair {
  vector<WordID> src;
  vector<WordID> trg;
  vector<LexicalAlignment> a;
  Array2D<short> posterior;
};

struct HierarchicalWordBase {
  explicit HierarchicalWordBase(const unsigned vocab_e_size) :
      base(prob_t::One()), r(1,1,1,1,0.66,50.0), u0(-log(vocab_e_size)), l(1,prob_t::One()), v(1, prob_t::Zero()) {}

  void ResampleHyperparameters(MT19937* rng) {
    r.resample_hyperparameters(rng);
  }

  inline double logp0(const vector<WordID>& s) const {
    return Md::log_poisson(s.size(), 7.5) + s.size() * u0;
  }

  // return p0 of rule.e_
  prob_t operator()(const TRule& rule) const {
    v[0].logeq(logp0(rule.e_));
    return r.prob(rule.e_, v.begin(), l.begin());
  }

  void Increment(const TRule& rule) {
    v[0].logeq(logp0(rule.e_));
    if (r.increment(rule.e_, v.begin(), l.begin(), &*prng).count) {
      base *= v[0] * l[0];
    }
  }

  void Decrement(const TRule& rule) {
    if (r.decrement(rule.e_, &*prng).count) {
      base /= prob_t(exp(logp0(rule.e_)));
    }
  }

  prob_t Likelihood() const {
    prob_t p; p.logeq(r.log_crp_prob());
    p *= base;
    return p;
  }

  void Summary() const {
    cerr << "NUMBER OF CUSTOMERS: " << r.num_customers() << "  (d=" << r.discount() << ",s=" << r.strength() << ')' << endl;
    for (MFCR<1,vector<WordID> >::const_iterator it = r.begin(); it != r.end(); ++it)
      cerr << "   " << it->second.total_dish_count_ << " (on " << it->second.table_counts_.size() << " tables) " << TD::GetString(it->first) << endl;
  }

  prob_t base;
  MFCR<1,vector<WordID> > r;
  const double u0;
  const vector<prob_t> l;
  mutable vector<prob_t> v;
};

struct BasicLexicalAlignment {
  explicit BasicLexicalAlignment(const vector<vector<WordID> >& lets,
                                 const unsigned words_e,
                                 const unsigned letters_e,
                                 vector<AlignedSentencePair>* corp) :
      letters(lets),
      corpus(*corp),
      //up0(words_e),
      //up0("en.chars.1gram", letters_e),
      //up0("en.words.1gram"),
      up0(letters_e),
      //up0("en.chars.2gram"),
      tmodel(up0) {
  }

  void InstantiateRule(const WordID src,
                       const WordID trg,
                       TRule* rule) const {
    static const WordID kX = TD::Convert("X") * -1;
    rule->lhs_ = kX;
    rule->e_ = letters[trg];
    rule->f_ = letters[src];
  }

  void InitializeRandom() {
    const WordID kNULL = TD::Convert("NULL");
    cerr << "Initializing with random alignments ...\n";
    for (unsigned i = 0; i < corpus.size(); ++i) {
      AlignedSentencePair& asp = corpus[i];
      asp.a.resize(asp.trg.size());
      for (unsigned j = 0; j < asp.trg.size(); ++j) {
        const unsigned char a_j = prng->next() * (1 + asp.src.size());
        const WordID f_a_j = (a_j ? asp.src[a_j - 1] : kNULL);
        TRule r;
        InstantiateRule(f_a_j, asp.trg[j], &r);
        asp.a[j].is_transliteration = false;
        asp.a[j].src_index = a_j;
        if (tmodel.IncrementRule(r, &*prng))
          up0.Increment(r);
      }
    }
    cerr << "  LLH = " << Likelihood() << endl;
  }

  prob_t Likelihood() const {
    prob_t p = tmodel.Likelihood();
    p *= up0.Likelihood();
    return p;
  }

  void ResampleHyperparemeters() {
    tmodel.ResampleHyperparameters(&*prng);
    up0.ResampleHyperparameters(&*prng);
    cerr << "  (base d=" << up0.r.discount() << ",s=" << up0.r.strength() << ")\n";
  }

  void ResampleCorpus();

  const vector<vector<WordID> >& letters; // spelling dictionary
  vector<AlignedSentencePair>& corpus;
  //PhraseConditionalUninformativeBase up0;
  //PhraseConditionalUninformativeUnigramBase up0;
  //UnigramWordBase up0;
  //HierarchicalUnigramBase up0;
  HierarchicalWordBase up0;
  //CompletelyUniformBase up0;
  //FixedNgramBase up0;
  //ConditionalTranslationModel<PhraseConditionalUninformativeBase> tmodel;
  //ConditionalTranslationModel<PhraseConditionalUninformativeUnigramBase> tmodel;
  //ConditionalTranslationModel<UnigramWordBase> tmodel;
  //ConditionalTranslationModel<HierarchicalUnigramBase> tmodel;
  MConditionalTranslationModel<HierarchicalWordBase> tmodel;
  //ConditionalTranslationModel<FixedNgramBase> tmodel;
  //ConditionalTranslationModel<CompletelyUniformBase> tmodel;
};

void BasicLexicalAlignment::ResampleCorpus() {
  static const WordID kNULL = TD::Convert("NULL");
  for (unsigned i = 0; i < corpus.size(); ++i) {
    AlignedSentencePair& asp = corpus[i];
    SampleSet<prob_t> ss; ss.resize(asp.src.size() + 1);
    for (unsigned j = 0; j < asp.trg.size(); ++j) {
      TRule r;
      unsigned char& a_j = asp.a[j].src_index;
      WordID f_a_j = (a_j ? asp.src[a_j - 1] : kNULL);
      InstantiateRule(f_a_j, asp.trg[j], &r);
      if (tmodel.DecrementRule(r, &*prng))
        up0.Decrement(r);

      for (unsigned prop_a_j = 0; prop_a_j <= asp.src.size(); ++prop_a_j) {
        const WordID prop_f = (prop_a_j ? asp.src[prop_a_j - 1] : kNULL);
        InstantiateRule(prop_f, asp.trg[j], &r);
        ss[prop_a_j] = tmodel.RuleProbability(r);
      }
      a_j = prng->SelectSample(ss);
      f_a_j = (a_j ? asp.src[a_j - 1] : kNULL);
      InstantiateRule(f_a_j, asp.trg[j], &r);
      if (tmodel.IncrementRule(r, &*prng))
        up0.Increment(r);
    }
  }
  cerr << "  LLH = " << Likelihood() << endl;
}

void ExtractLetters(const set<WordID>& v, vector<vector<WordID> >* l, set<WordID>* letset = NULL) {
  for (set<WordID>::const_iterator it = v.begin(); it != v.end(); ++it) {
    vector<WordID>& letters = (*l)[*it];
    if (letters.size()) continue;   // if e and f have the same word

    const string& w = TD::Convert(*it);
    
    size_t cur = 0;
    while (cur < w.size()) {
      const size_t len = UTF8Len(w[cur]);
      letters.push_back(TD::Convert(w.substr(cur, len)));
      if (letset) letset->insert(letters.back());
      cur += len;
    }
  }
}

void Debug(const AlignedSentencePair& asp) {
  cerr << TD::GetString(asp.src) << endl << TD::GetString(asp.trg) << endl;
  Array2D<bool> a(asp.src.size(), asp.trg.size());
  for (unsigned j = 0; j < asp.trg.size(); ++j)
    if (asp.a[j].src_index) a(asp.a[j].src_index - 1, j) = true;
  cerr << a << endl;
}

void AddSample(AlignedSentencePair* asp) {
  for (unsigned j = 0; j < asp->trg.size(); ++j)
    asp->posterior(asp->a[j].src_index, j)++;
}

void WriteAlignments(const AlignedSentencePair& asp) {
  bool first = true;
  for (unsigned j = 0; j < asp.trg.size(); ++j) {
    int src_index = -1;
    int mc = -1;
    for (unsigned i = 0; i <= asp.src.size(); ++i) {
      if (asp.posterior(i, j) > mc) {
        mc = asp.posterior(i, j);
        src_index = i;
      }
    }

    if (src_index) {
      if (first) first = false; else cout << ' ';
      cout << (src_index - 1) << '-' << j;
    }
  }
  cout << endl;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

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

  vector<AlignedSentencePair> corpus(corpuse.size());
  for (unsigned i = 0; i < corpuse.size(); ++i) {
    corpus[i].src.swap(corpusf[i]);
    corpus[i].trg.swap(corpuse[i]);
    corpus[i].posterior.resize(corpus[i].src.size() + 1, corpus[i].trg.size());
  }
  corpusf.clear(); corpuse.clear();

  vocabf.insert(TD::Convert("NULL"));
  vector<vector<WordID> > letters(TD::NumWords() + 1);
  set<WordID> letset;
  ExtractLetters(vocabe, &letters, &letset);
  ExtractLetters(vocabf, &letters, NULL);
  letters[TD::Convert("NULL")].clear();

  // TODO configure this
  const int max_src_chunk = conf["max_src_chunk"].as<unsigned>();
  const int max_trg_chunk = conf["max_trg_chunk"].as<unsigned>();
  const double s2t_rat = conf["expected_src_to_trg_ratio"].as<double>();
  const BackwardEstimator be(conf["s2t"].as<string>(), conf["t2s"].as<string>());
  Transliterations tl(max_src_chunk, max_trg_chunk, s2t_rat, be); 

  cerr << "Initializing transliteration graph structures ...\n";
  for (int i = 0; i < corpus.size(); ++i) {
    const vector<int>& src = corpus[i].src;
    const vector<int>& trg = corpus[i].trg;
    for (int j = 0; j < src.size(); ++j) {
      const vector<int>& src_let = letters[src[j]];
      for (int k = 0; k < trg.size(); ++k) {
        const vector<int>& trg_let = letters[trg[k]];
        tl.Initialize(src[j], src_let, trg[k], trg_let);
        //if (src_let.size() < min_trans_src)
        //  tl.Forbid(src[j], src_let, trg[k], trg_let);
      }
    }
  }
  cerr << endl;
  tl.GraphSummary();

  return 0;
}
