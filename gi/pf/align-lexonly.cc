#include <iostream>
#include <tr1/memory>
#include <queue>

#include <boost/multi_array.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

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
#include "ccrp_nt.h"
#include "corpus.h"
#include "ngram_base.h"

using namespace std;
using namespace tr1;
namespace po = boost::program_options;

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

shared_ptr<MT19937> prng;

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
      base(prob_t::One()), r(25,25,10), u0(-log(vocab_e_size)) {}

  void ResampleHyperparameters(MT19937* rng) {
    r.resample_hyperparameters(rng);
  }

  inline double logp0(const vector<WordID>& s) const {
    return s.size() * u0;
  }

  // return p0 of rule.e_
  prob_t operator()(const TRule& rule) const {
    prob_t p; p.logeq(r.logprob(rule.e_, logp0(rule.e_)));
    return p;
  }

  void Increment(const TRule& rule) {
    if (r.increment(rule.e_)) {
      prob_t p; p.logeq(logp0(rule.e_));
      base *= p;
    }
  }

  void Decrement(const TRule& rule) {
    if (r.decrement(rule.e_)) {
      prob_t p; p.logeq(logp0(rule.e_));
      base /= p;
    }
  }

  prob_t Likelihood() const {
    prob_t p; p.logeq(r.log_crp_prob());
    p *= base;
    return p;
  }

  void Summary() const {
    cerr << "NUMBER OF CUSTOMERS: " << r.num_customers() << "  (\\alpha=" << r.concentration() << ')' << endl;
    for (CCRP_NoTable<vector<WordID> >::const_iterator it = r.begin(); it != r.end(); ++it)
      cerr << "   " << it->second << '\t' << TD::GetString(it->first) << endl;
  }

  prob_t base;
  CCRP_NoTable<vector<WordID> > r;
  const double u0;
};

struct BasicLexicalAlignment {
  explicit BasicLexicalAlignment(const vector<vector<WordID> >& lets,
                                 const unsigned words_e,
                                 const unsigned letters_e,
                                 vector<AlignedSentencePair>* corp) :
      letters(lets),
      corpus(*corp),
      up0("fr-en.10k.translit-base.txt.gz"),
      //up0(words_e),
      //up0("en.chars.1gram", letters_e),
      //up0("en.words.1gram"),
      //up0(letters_e),
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
        if (tmodel.IncrementRule(r))
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
    cerr << "  LLH_prev = " << Likelihood() << flush;
    tmodel.ResampleHyperparameters(&*prng);
    up0.ResampleHyperparameters(&*prng);
    cerr << "\tLLH_post = " << Likelihood() << endl;
  }

  void ResampleCorpus();

  const vector<vector<WordID> >& letters; // spelling dictionary
  vector<AlignedSentencePair>& corpus;
  //PhraseConditionalUninformativeBase up0;
  //PhraseConditionalUninformativeUnigramBase up0;
  //UnigramWordBase up0;
  //HierarchicalUnigramBase up0;
  TableLookupBase up0;
  //HierarchicalWordBase up0;
  //PoissonUniformUninformativeBase up0;
  //CompletelyUniformBase up0;
  //FixedNgramBase up0;
  //ConditionalTranslationModel<PhraseConditionalUninformativeBase> tmodel;
  //ConditionalTranslationModel<PhraseConditionalUninformativeUnigramBase> tmodel;
  //ConditionalTranslationModel<UnigramWordBase> tmodel;
  //ConditionalTranslationModel<HierarchicalUnigramBase> tmodel;
  //ConditionalTranslationModel<HierarchicalWordBase> tmodel;
  //ConditionalTranslationModel<PoissonUniformUninformativeBase> tmodel;
  ConditionalTranslationModel<TableLookupBase> tmodel;
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
      if (tmodel.DecrementRule(r))
        up0.Decrement(r);

      for (unsigned prop_a_j = 0; prop_a_j <= asp.src.size(); ++prop_a_j) {
        const WordID prop_f = (prop_a_j ? asp.src[prop_a_j - 1] : kNULL);
        InstantiateRule(prop_f, asp.trg[j], &r);
        ss[prop_a_j] = tmodel.RuleProbability(r);
      }
      a_j = prng->SelectSample(ss);
      f_a_j = (a_j ? asp.src[a_j - 1] : kNULL);
      InstantiateRule(f_a_j, asp.trg[j], &r);
      if (tmodel.IncrementRule(r))
        up0.Increment(r);
    }
  }
  cerr << "  LLH = " << tmodel.Likelihood() << endl;
}

void ExtractLetters(const set<WordID>& v, vector<vector<WordID> >* l, set<WordID>* letset = NULL) {
  for (set<WordID>::const_iterator it = v.begin(); it != v.end(); ++it) {
    if (*it >= l->size()) { l->resize(*it + 1); }
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
  vector<vector<WordID> > letters(TD::NumWords());
  set<WordID> letset;
  ExtractLetters(vocabe, &letters, &letset);
  ExtractLetters(vocabf, &letters, NULL);
  letters[TD::Convert("NULL")].clear();

  BasicLexicalAlignment x(letters, vocabe.size(), letset.size(), &corpus);
  x.InitializeRandom();
  const unsigned samples = conf["samples"].as<unsigned>();
  for (int i = 0; i < samples; ++i) {
    for (int j = 395; j < 397; ++j) Debug(corpus[j]);
    cerr << i << "\t" << x.tmodel.r.size() << "\t";
    if (i % 10 == 0) x.ResampleHyperparemeters();
    x.ResampleCorpus();
    if (i > (samples / 5) && (i % 10 == 9)) for (int j = 0; j < corpus.size(); ++j) AddSample(&corpus[j]);
  }
  for (unsigned i = 0; i < corpus.size(); ++i)
    WriteAlignments(corpus[i]);
  //ModelAndData posterior(x, &corpus, vocabe, vocabf);
  x.tmodel.Summary();
  x.up0.Summary();

  //posterior.Sample();

  return 0;
}
