#include <iostream>
#include <tr1/unordered_map>

#include <boost/shared_ptr.hpp>
#include <boost/functional.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "tdict.h"
#include "filelib.h"
#include "sampler.h"
#include "ccrp_onetable.h"
#include "array2d.h"

using namespace std;
using namespace std::tr1;
namespace po = boost::program_options;

void PrintTopCustomers(const CCRP_OneTable<WordID>& crp) {
  for (CCRP_OneTable<WordID>::const_iterator it = crp.begin(); it != crp.end(); ++it) {
    cerr << "  " << TD::Convert(it->first) << " = " << it->second << endl;
  }
}

void PrintAlignment(const vector<WordID>& src, const vector<WordID>& trg, const vector<unsigned char>& a) {
  cerr << TD::GetString(src) << endl << TD::GetString(trg) << endl;
  Array2D<bool> al(src.size(), trg.size());
  for (int i = 0; i < a.size(); ++i)
    if (a[i] != 255) al(a[i], i) = true;
  cerr << al << endl;
}

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

struct Unigram;
struct Bigram {
  Bigram() : trg(), cond() {}
  Bigram(WordID prev, WordID cur, WordID t) : trg(t) { cond.first = prev; cond.second = cur; }
  const pair<WordID,WordID>& ConditioningPair() const {
    return cond;
  }
  WordID& prev_src() { return cond.first; }
  WordID& cur_src() { return cond.second; }
  const WordID& prev_src() const { return cond.first; }
  const WordID& cur_src() const { return cond.second; }
  WordID trg;
 private:
  pair<WordID, WordID> cond;
};

struct Unigram {
  Unigram() : cur_src(), trg() {}
  Unigram(WordID s, WordID t) : cur_src(s), trg(t) {}
  WordID cur_src;
  WordID trg;
};

ostream& operator<<(ostream& os, const Bigram& b) {
  os << "( " << TD::Convert(b.trg) << " | " << TD::Convert(b.prev_src()) << " , " << TD::Convert(b.cur_src()) << " )";
  return os;
}

ostream& operator<<(ostream& os, const Unigram& u) {
  os << "( " << TD::Convert(u.trg) << " | " << TD::Convert(u.cur_src) << " )";
  return os;
}

bool operator==(const Bigram& a, const Bigram& b) {
  return a.trg == b.trg && a.cur_src() == b.cur_src() && a.prev_src() == b.prev_src();
}

bool operator==(const Unigram& a, const Unigram& b) {
  return a.trg == b.trg && a.cur_src == b.cur_src;
}

size_t hash_value(const Bigram& b) {
  size_t h = boost::hash_value(b.prev_src());
  boost::hash_combine(h, boost::hash_value(b.cur_src()));
  boost::hash_combine(h, boost::hash_value(b.trg));
  return h;
}

size_t hash_value(const Unigram& u) {
  size_t h = boost::hash_value(u.cur_src);
  boost::hash_combine(h, boost::hash_value(u.trg));
  return h;
}

void ReadParallelCorpus(const string& filename,
                vector<vector<WordID> >* f,
                vector<vector<WordID> >* e,
                set<WordID>* vocab_f,
                set<WordID>* vocab_e) {
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

struct UnigramModel {
  UnigramModel(size_t src_voc_size, size_t trg_voc_size) :
    unigrams(TD::NumWords() + 1, CCRP_OneTable<WordID>(1,1,1,1)),
    p0(1.0 / trg_voc_size) {}

  void increment(const Bigram& b) {
    unigrams[b.cur_src()].increment(b.trg);
  }

  void decrement(const Bigram& b) {
    unigrams[b.cur_src()].decrement(b.trg);
  }

  double prob(const Bigram& b) const {
    const double q0 = unigrams[b.cur_src()].prob(b.trg, p0);
    return q0;
  }

  double LogLikelihood() const {
    double llh = 0;
    for (unsigned i = 0; i < unigrams.size(); ++i) {
      const CCRP_OneTable<WordID>& crp = unigrams[i];
      if (crp.num_customers() > 0) {
        llh += crp.log_crp_prob();
        llh += crp.num_tables() * log(p0);
      }
    }
    return llh;
  }

  void ResampleHyperparameters(MT19937* rng) {
    for (unsigned i = 0; i < unigrams.size(); ++i)
      unigrams[i].resample_hyperparameters(rng);
  }

  vector<CCRP_OneTable<WordID> > unigrams;  // unigrams[src].prob(trg, p0) = p(trg|src)

  const double p0;
};

struct BigramModel {
  BigramModel(size_t src_voc_size, size_t trg_voc_size) :
    unigrams(TD::NumWords() + 1, CCRP_OneTable<WordID>(1,1,1,1)),
    p0(1.0 / trg_voc_size) {}

  void increment(const Bigram& b) {
    BigramMap::iterator it = bigrams.find(b.ConditioningPair());
    if (it == bigrams.end()) {
      it = bigrams.insert(make_pair(b.ConditioningPair(), CCRP_OneTable<WordID>(1,1,1,1))).first;
    }
    if (it->second.increment(b.trg))
      unigrams[b.cur_src()].increment(b.trg);
  }

  void decrement(const Bigram& b) {
    BigramMap::iterator it = bigrams.find(b.ConditioningPair());
    assert(it != bigrams.end());
    if (it->second.decrement(b.trg)) {
      unigrams[b.cur_src()].decrement(b.trg);
      if (it->second.num_customers() == 0)
        bigrams.erase(it);
    }
  }

  double prob(const Bigram& b) const {
    const double q0 = unigrams[b.cur_src()].prob(b.trg, p0);
    const BigramMap::const_iterator it = bigrams.find(b.ConditioningPair());
    if (it == bigrams.end()) return q0;
    return it->second.prob(b.trg, q0);
  }

  double LogLikelihood() const {
    double llh = 0;
    for (unsigned i = 0; i < unigrams.size(); ++i) {
      const CCRP_OneTable<WordID>& crp = unigrams[i];
      if (crp.num_customers() > 0) {
        llh += crp.log_crp_prob();
        llh += crp.num_tables() * log(p0);
      }
    }
    for (BigramMap::const_iterator it = bigrams.begin(); it != bigrams.end(); ++it) {
      const CCRP_OneTable<WordID>& crp = it->second;
      const WordID cur_src = it->first.second;
      llh += crp.log_crp_prob();
      for (CCRP_OneTable<WordID>::const_iterator bit = crp.begin(); bit != crp.end(); ++bit) {
        llh += log(unigrams[cur_src].prob(bit->second, p0));
      }
    }
    return llh;
  }

  void ResampleHyperparameters(MT19937* rng) {
    for (unsigned i = 0; i < unigrams.size(); ++i)
      unigrams[i].resample_hyperparameters(rng);
    for (BigramMap::iterator it = bigrams.begin(); it != bigrams.end(); ++it)
      it->second.resample_hyperparameters(rng);
  }

  typedef unordered_map<pair<WordID,WordID>, CCRP_OneTable<WordID>, boost::hash<pair<WordID,WordID> > > BigramMap;
  BigramMap bigrams;  // bigrams[(src-1,src)].prob(trg, q0) = p(trg|src,src-1)
  vector<CCRP_OneTable<WordID> > unigrams;  // unigrams[src].prob(trg, p0) = p(trg|src)

  const double p0;
};

struct BigramAlignmentModel {
  BigramAlignmentModel(size_t src_voc_size, size_t trg_voc_size) : bigrams(TD::NumWords() + 1, CCRP_OneTable<WordID>(1,1,1,1)), p0(1.0 / src_voc_size) {}
  void increment(WordID prev, WordID next) {
    bigrams[prev].increment(next);  // hierarchy?
  }
  void decrement(WordID prev, WordID next) {
    bigrams[prev].decrement(next);  // hierarchy?
  }
  double prob(WordID prev, WordID next) {
    return bigrams[prev].prob(next, p0);
  }
  double LogLikelihood() const {
    double llh = 0;
    for (unsigned i = 0; i < bigrams.size(); ++i) {
      const CCRP_OneTable<WordID>& crp = bigrams[i];
      if (crp.num_customers() > 0) {
        llh += crp.log_crp_prob();
        llh += crp.num_tables() * log(p0);
      }
    }
    return llh;
  }

  vector<CCRP_OneTable<WordID> > bigrams;  // bigrams[prev].prob(next, p0) = p(next|prev)
  const double p0;
};

struct Alignment {
  vector<unsigned char> a;
};

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const unsigned samples = conf["samples"].as<unsigned>();

  boost::shared_ptr<MT19937> prng;
  if (conf.count("random_seed"))
    prng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    prng.reset(new MT19937);
  MT19937& rng = *prng;

  vector<vector<WordID> > corpuse, corpusf;
  set<WordID> vocabe, vocabf;
  cerr << "Reading corpus...\n";
  ReadParallelCorpus(conf["input"].as<string>(), &corpusf, &corpuse, &vocabf, &vocabe);
  cerr << "F-corpus size: " << corpusf.size() << " sentences\t (" << vocabf.size() << " word types)\n";
  cerr << "E-corpus size: " << corpuse.size() << " sentences\t (" << vocabe.size() << " word types)\n";
  assert(corpusf.size() == corpuse.size());
  const size_t corpus_len = corpusf.size();
  const WordID kNULL = TD::Convert("<eps>");
  const WordID kBOS = TD::Convert("<s>");
  const WordID kEOS = TD::Convert("</s>");
  Bigram TT(kBOS, TD::Convert("我"), TD::Convert("i"));
  Bigram TT2(kBOS, TD::Convert("要"), TD::Convert("i"));

  UnigramModel model(vocabf.size(), vocabe.size());
  vector<Alignment> alignments(corpus_len);
  for (unsigned ci = 0; ci < corpus_len; ++ci) {
    const vector<WordID>& src = corpusf[ci];
    const vector<WordID>& trg = corpuse[ci];
    vector<unsigned char>& alg = alignments[ci].a;
    alg.resize(trg.size());
    int lenp1 = src.size() + 1;
    WordID prev_src = kBOS;
    for (int j = 0; j < trg.size(); ++j) {
      int samp = lenp1 * rng.next();
      --samp;
      if (samp < 0) samp = 255;
      alg[j] = samp;
      WordID cur_src = (samp == 255 ? kNULL : src[alg[j]]);
      Bigram b(prev_src, cur_src, trg[j]);
      model.increment(b);
      prev_src = cur_src;
    }
    Bigram b(prev_src, kEOS, kEOS);
    model.increment(b);
  }
  cerr << "Initial LLH: " << model.LogLikelihood() << endl;

  SampleSet<double> ss;
  for (unsigned si = 0; si < 50; ++si) {
    for (unsigned ci = 0; ci < corpus_len; ++ci) {
      const vector<WordID>& src = corpusf[ci];
      const vector<WordID>& trg = corpuse[ci];
      vector<unsigned char>& alg = alignments[ci].a;
      WordID prev_src = kBOS;
      for (unsigned j = 0; j < trg.size(); ++j) {
        unsigned char& a_j = alg[j];
        WordID cur_e_a_j = (a_j == 255 ? kNULL : src[a_j]);
        Bigram b(prev_src, cur_e_a_j, trg[j]);
        //cerr << "DEC: " << b << "\t" << nextb << endl;
        model.decrement(b);
        ss.clear();
        for (unsigned i = 0; i <= src.size(); ++i) {
          const WordID cur_src = (i ? src[i-1] : kNULL);
          b.cur_src() = cur_src;
          ss.add(model.prob(b));
        }
        int sampled_a_j = rng.SelectSample(ss);
        a_j = (sampled_a_j ? sampled_a_j - 1 : 255);
        cur_e_a_j = (a_j == 255 ? kNULL : src[a_j]);
        b.cur_src() = cur_e_a_j;
        //cerr << "INC: " << b << "\t" << nextb << endl;
        model.increment(b);
        prev_src = cur_e_a_j;
      }
    }
    cerr << '.' << flush;
    if (si % 10 == 9) {
      cerr << "[LLH prev=" << model.LogLikelihood();
      //model.ResampleHyperparameters(&rng);
      cerr << " new=" << model.LogLikelihood() << "]\n";
      //pair<WordID,WordID> xx = make_pair(kBOS, TD::Convert("我"));
      //PrintTopCustomers(model.bigrams.find(xx)->second);
      cerr << "p(" << TT << ") = " << model.prob(TT) << endl;
      cerr << "p(" << TT2 << ") = " << model.prob(TT2) << endl;
      PrintAlignment(corpusf[0], corpuse[0], alignments[0].a);
    }
  }
  {
  // MODEL 2
  BigramModel model(vocabf.size(), vocabe.size());
  BigramAlignmentModel amodel(vocabf.size(), vocabe.size());
  for (unsigned ci = 0; ci < corpus_len; ++ci) {
    const vector<WordID>& src = corpusf[ci];
    const vector<WordID>& trg = corpuse[ci];
    vector<unsigned char>& alg = alignments[ci].a;
    WordID prev_src = kBOS;
    for (int j = 0; j < trg.size(); ++j) {
      WordID cur_src = (alg[j] == 255 ? kNULL : src[alg[j]]);
      Bigram b(prev_src, cur_src, trg[j]);
      model.increment(b);
      amodel.increment(prev_src, cur_src);
      prev_src = cur_src;
    }
    amodel.increment(prev_src, kEOS);
    Bigram b(prev_src, kEOS, kEOS);
    model.increment(b);
  }
  cerr << "Initial LLH: " << model.LogLikelihood() << " " << amodel.LogLikelihood() << endl;

  SampleSet<double> ss;
  for (unsigned si = 0; si < samples; ++si) {
    for (unsigned ci = 0; ci < corpus_len; ++ci) {
      const vector<WordID>& src = corpusf[ci];
      const vector<WordID>& trg = corpuse[ci];
      vector<unsigned char>& alg = alignments[ci].a;
      WordID prev_src = kBOS;
      for (unsigned j = 0; j < trg.size(); ++j) {
        unsigned char& a_j = alg[j];
        WordID cur_e_a_j = (a_j == 255 ? kNULL : src[a_j]);
        Bigram b(prev_src, cur_e_a_j, trg[j]);
        WordID next_src = kEOS;
        WordID next_trg = kEOS;
        if (j < (trg.size() - 1)) {
          next_src = (alg[j+1] == 255 ? kNULL : src[alg[j + 1]]);
          next_trg = trg[j + 1];
        }
        Bigram nextb(cur_e_a_j, next_src, next_trg);
        //cerr << "DEC: " << b << "\t" << nextb << endl;
        model.decrement(b);
        model.decrement(nextb);
        amodel.decrement(prev_src, cur_e_a_j);
        amodel.decrement(cur_e_a_j, next_src);
        ss.clear();
        for (unsigned i = 0; i <= src.size(); ++i) {
          const WordID cur_src = (i ? src[i-1] : kNULL);
          b.cur_src() = cur_src;
          ss.add(model.prob(b) * model.prob(nextb) * amodel.prob(prev_src, cur_src) * amodel.prob(cur_src, next_src));
          //cerr << log(ss[ss.size() - 1]) << "\t" << b << endl;
        }
        int sampled_a_j = rng.SelectSample(ss);
        a_j = (sampled_a_j ? sampled_a_j - 1 : 255);
        cur_e_a_j = (a_j == 255 ? kNULL : src[a_j]);
        b.cur_src() = cur_e_a_j;
        nextb.prev_src() = cur_e_a_j;
        //cerr << "INC: " << b << "\t" << nextb << endl;
        //exit(1);
        model.increment(b);
        model.increment(nextb);
        amodel.increment(prev_src, cur_e_a_j);
        amodel.increment(cur_e_a_j, next_src);
        prev_src = cur_e_a_j;
      }
    }
    cerr << '.' << flush;
    if (si % 10 == 9) {
      cerr << "[LLH prev=" << (model.LogLikelihood() + amodel.LogLikelihood());
      //model.ResampleHyperparameters(&rng);
      cerr << " new=" << model.LogLikelihood() << "]\n";
      pair<WordID,WordID> xx = make_pair(kBOS, TD::Convert("我"));
      cerr << "p(" << TT << ") = " << model.prob(TT) << endl;
      cerr << "p(" << TT2 << ") = " << model.prob(TT2) << endl;
      pair<WordID,WordID> xx2 = make_pair(kBOS, TD::Convert("要"));
      PrintTopCustomers(model.bigrams.find(xx)->second);
      //PrintTopCustomers(amodel.bigrams[TD::Convert("<s>")]);
      //PrintTopCustomers(model.unigrams[TD::Convert("<eps>")]);
      PrintAlignment(corpusf[0], corpuse[0], alignments[0].a);
    }
  }
  }
  return 0;
}

