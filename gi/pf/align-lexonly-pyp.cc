#include <iostream>
#include <queue>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "tdict.h"
#include "stringlib.h"
#include "filelib.h"
#include "array2d.h"
#include "sampler.h"
#include "corpus.h"
#include "pyp_tm.h"
#include "hpyp_tm.h"
#include "quasi_model2.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,s",po::value<unsigned>()->default_value(1000),"Number of samples")
        ("infer_alignment_hyperparameters,I", "Infer alpha and p_null, otherwise fixed values will be assumed")
        ("p_null,0", po::value<double>()->default_value(0.08), "probability of aligning to null")
        ("align_alpha,a", po::value<double>()->default_value(4.0), "how 'tight' is the bias toward be along the diagonal?")
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

MT19937* prng;

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

template <class LexicalTranslationModel>
struct Aligner {
  Aligner(const vector<vector<WordID> >& lets,
          int vocab_size,
          int num_letters,
          const po::variables_map& conf,
          vector<AlignedSentencePair>* c) :
      corpus(*c),
      paj_model(conf["align_alpha"].as<double>(), conf["p_null"].as<double>()),
      infer_paj(conf.count("infer_alignment_hyperparameters") > 0),
      model(lets, vocab_size, num_letters),
      kNULL(TD::Convert("NULL")) {
    assert(lets[kNULL].size() == 0);
  }

  vector<AlignedSentencePair>& corpus;
  QuasiModel2 paj_model;
  const bool infer_paj;
  LexicalTranslationModel model;
  const WordID kNULL;

  void ResampleHyperparameters() {
    model.ResampleHyperparameters(prng);
    if (infer_paj) paj_model.ResampleHyperparameters(prng);
  }

  void InitializeRandom() {
    cerr << "Initializing with random alignments ...\n";
    for (unsigned i = 0; i < corpus.size(); ++i) {
      AlignedSentencePair& asp = corpus[i];
      asp.a.resize(asp.trg.size());
      for (unsigned j = 0; j < asp.trg.size(); ++j) {
        unsigned char& a_j = asp.a[j].src_index;
        a_j = prng->next() * (1 + asp.src.size());
        const WordID f_a_j = (a_j ? asp.src[a_j - 1] : kNULL);
        model.Increment(f_a_j, asp.trg[j], &*prng);
        paj_model.Increment(a_j, j, asp.src.size(), asp.trg.size());
      }
    }
    cerr << "Corpus intialized randomly." << endl;
    cerr << "LLH = " << Likelihood() << "    \t(Amodel=" << paj_model.Likelihood()
         << " TModel=" << model.Likelihood() << ") contexts=" << model.UniqueConditioningContexts() << endl;
  }

  void ResampleCorpus() {
    for (unsigned i = 0; i < corpus.size(); ++i) {
      AlignedSentencePair& asp = corpus[i];
      SampleSet<prob_t> ss; ss.resize(asp.src.size() + 1);
      for (unsigned j = 0; j < asp.trg.size(); ++j) {
        unsigned char& a_j = asp.a[j].src_index;
        const WordID e_j = asp.trg[j];
        WordID f_a_j = (a_j ? asp.src[a_j - 1] : kNULL);
        model.Decrement(f_a_j, e_j, prng);
        paj_model.Decrement(a_j, j, asp.src.size(), asp.trg.size());

        for (unsigned prop_a_j = 0; prop_a_j <= asp.src.size(); ++prop_a_j) {
          const WordID prop_f = (prop_a_j ? asp.src[prop_a_j - 1] : kNULL);
          ss[prop_a_j] = model.Prob(prop_f, e_j);
          ss[prop_a_j] *= paj_model.Prob(prop_a_j, j, asp.src.size(), asp.trg.size());
        }
        a_j = prng->SelectSample(ss);
        f_a_j = (a_j ? asp.src[a_j - 1] : kNULL);
        model.Increment(f_a_j, e_j, prng);
        paj_model.Increment(a_j, j, asp.src.size(), asp.trg.size());
      }
    }
  }

  prob_t Likelihood() const {
    return model.Likelihood() * paj_model.Likelihood();
  }
};

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
  for (unsigned j = 0; j < asp.trg.size(); ++j) {
    assert(asp.a[j].src_index <= asp.src.size());
    if (asp.a[j].src_index) a(asp.a[j].src_index - 1, j) = true;
  }
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
    prng = new MT19937(conf["random_seed"].as<uint32_t>());
  else
    prng = new MT19937;

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

  //Aligner<PYPLexicalTranslation> aligner(letters, vocabe.size(), letset.size(), conf, &corpus);
  Aligner<HPYPLexicalTranslation> aligner(letters, vocabe.size(), letset.size(), conf, &corpus);
  aligner.InitializeRandom();

  const unsigned samples = conf["samples"].as<unsigned>();
  for (int i = 0; i < samples; ++i) {
    for (int j = 65; j < 67; ++j) Debug(corpus[j]);
    if (i % 10 == 9) {
      aligner.ResampleHyperparameters();
      cerr << "LLH = " << aligner.Likelihood() << "    \t(Amodel=" << aligner.paj_model.Likelihood()
           << " TModel=" << aligner.model.Likelihood() << ") contexts=" << aligner.model.UniqueConditioningContexts() << endl;
    }
    aligner.ResampleCorpus();
    if (i > (samples / 5) && (i % 6 == 5)) for (int j = 0; j < corpus.size(); ++j) AddSample(&corpus[j]);
  }
  for (unsigned i = 0; i < corpus.size(); ++i)
    WriteAlignments(corpus[i]);
  aligner.model.Summary();

  return 0;
}
