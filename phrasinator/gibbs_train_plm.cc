#include <iostream>
#include <tr1/memory>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "filelib.h"
#include "dict.h"
#include "sampler.h"
#include "ccrp.h"
#include "m.h"

using namespace std;
using namespace std::tr1;
namespace po = boost::program_options;

Dict d; // global dictionary

string Join(char joiner, const vector<int>& phrase) {
  ostringstream os;
  for (int i = 0; i < phrase.size(); ++i) {
    if (i > 0) os << joiner;
    os << d.Convert(phrase[i]);
  }
  return os.str();
}

ostream& operator<<(ostream& os, const vector<int>& phrase) {
  for (int i = 0; i < phrase.size(); ++i)
    os << (i == 0 ? "" : " ") << d.Convert(phrase[i]);
  return os;
}

struct UnigramLM {
  explicit UnigramLM(const string& fname) {
    ifstream in(fname.c_str());
    assert(in);
  }

  double logprob(int word) const {
    assert(word < freqs_.size());
    return freqs_[word];
  }

  vector<double> freqs_;
};

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("samples,s",po::value<unsigned>()->default_value(1000),"Number of samples")
        ("input,i",po::value<string>(),"Read file from")
        ("random_seed,S",po::value<uint32_t>(), "Random seed")
        ("write_cdec_grammar,g", po::value<string>(), "Write cdec grammar to this file")
        ("write_cdec_weights,w", po::value<string>(), "Write cdec weights to this file")
        ("poisson_length,p", "Use a Poisson distribution as the length of a phrase in the base distribuion")
        ("no_hyperparameter_inference,N", "Disable hyperparameter inference");
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

void ReadCorpus(const string& filename, vector<vector<int> >* c, set<int>* vocab) {
  c->clear();
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
    c->push_back(vector<int>());
    vector<int>& v = c->back();
    d.ConvertWhitespaceDelimitedLine(line, &v);
    for (int i = 0; i < v.size(); ++i) vocab->insert(v[i]);
  }
  if (in != &cin) delete in;
}

struct UniphraseLM {
  UniphraseLM(const vector<vector<int> >& corpus,
              const set<int>& vocab,
              const po::variables_map& conf) :
    phrases_(1,1,1,1),
    gen_(1,1,1,1),
    corpus_(corpus),
    uniform_word_(1.0 / vocab.size()),
    gen_p0_(0.5),
    p_end_(0.5),
    use_poisson_(conf.count("poisson_length") > 0) {}

  double p0(const vector<int>& phrase) const {
    static vector<double> p0s(10000, 0.0);
    assert(phrase.size() < 10000);
    double& p = p0s[phrase.size()];
    if (p) return p;
    p = exp(log_p0(phrase));
    if (!p) {
      cerr << "0 prob phrase: " << phrase << "\nAssigning std::numeric_limits<double>::min()\n";
      p = std::numeric_limits<double>::min();
    }
    return p;
  }

  double log_p0(const vector<int>& phrase) const {
    double len_logprob;
    if (use_poisson_)
      len_logprob = Md::log_poisson(phrase.size(), 1.0);
    else
      len_logprob = log(1 - p_end_) * (phrase.size() -1) + log(p_end_);
    return log(uniform_word_) * phrase.size() + len_logprob;
  }

  double llh() const {
    double llh = gen_.log_crp_prob();
    llh += gen_.num_tables(false) * log(gen_p0_) +
           gen_.num_tables(true) * log(1 - gen_p0_);
    double llhr = phrases_.log_crp_prob();
    for (CCRP<vector<int> >::const_iterator it = phrases_.begin(); it != phrases_.end(); ++it) {
      llhr += phrases_.num_tables(it->first) * log_p0(it->first);
      //llhr += log_p0(it->first);
      if (!isfinite(llh)) {
        cerr << it->first << endl;
        cerr << log_p0(it->first) << endl;
        abort();
      }
    }
    return llh + llhr;
  }

  void Sample(unsigned int samples, bool hyp_inf, MT19937* rng) {
    cerr << "Initializing...\n";
    z_.resize(corpus_.size());
    int tc = 0;
    for (int i = 0; i < corpus_.size(); ++i) {
      const vector<int>& line = corpus_[i];
      const int ls = line.size();
      const int last_pos = ls - 1;
      vector<bool>& z = z_[i];
      z.resize(ls);
      int prev = 0;
      for (int j = 0; j < ls; ++j) {
        z[j] = rng->next() < 0.5;
        if (j == last_pos) z[j] = true;  // break phrase at the end of the sentence
        if (z[j]) {
          const vector<int> p(line.begin() + prev, line.begin() + j + 1);
          phrases_.increment(p, p0(p), rng);
          //cerr << p << ": " << p0(p) << endl;
          prev = j + 1;
          gen_.increment(false, gen_p0_, rng);
          ++tc; // remove
        }
      }
      ++tc;
      gen_.increment(true, 1.0 - gen_p0_, rng); // end of utterance
    }
    cerr << "TC: " << tc << endl;
    cerr << "Initial LLH: " << llh() << endl;
    cerr << "Sampling...\n";
    cerr << gen_ << endl;
    for (int s = 1; s < samples; ++s) {
      cerr << '.';
      if (s % 10 == 0) {
        cerr << " [" << s;
        if (hyp_inf) ResampleHyperparameters(rng);
        cerr << " LLH=" << llh() << "]\n";
        vector<int> z(z_[0].size(), 0);
        //for (int j = 0; j < z.size(); ++j) z[j] = z_[0][j];
        //SegCorpus::Write(corpus_[0], z, d);
      }
      for (int i = 0; i < corpus_.size(); ++i) {
        const vector<int>& line = corpus_[i];
        const int ls = line.size();
        const int last_pos = ls - 1;
        vector<bool>& z = z_[i];
        int prev = 0;
        for (int j = 0; j < last_pos; ++j) { // don't resample last position
          int next = j+1;  while(!z[next]) { ++next; }
          const vector<int> p1p2(line.begin() + prev, line.begin() + next + 1);
          const vector<int> p1(line.begin() + prev, line.begin() + j + 1);
          const vector<int> p2(line.begin() + j + 1, line.begin() + next + 1);

          if (z[j]) {
            phrases_.decrement(p1, rng);
            phrases_.decrement(p2, rng);
            gen_.decrement(false, rng);
            gen_.decrement(false, rng);
          } else {
            phrases_.decrement(p1p2, rng);
            gen_.decrement(false, rng);
          }

          const double d1 = phrases_.prob(p1p2, p0(p1p2)) * gen_.prob(false, gen_p0_);
          double d2 = phrases_.prob(p1, p0(p1)) * gen_.prob(false, gen_p0_);
          phrases_.increment(p1, p0(p1), rng);
          gen_.increment(false, gen_p0_, rng);
          d2 *= phrases_.prob(p2, p0(p2)) * gen_.prob(false, gen_p0_);
          phrases_.decrement(p1, rng);
          gen_.decrement(false, rng);
          z[j] = rng->SelectSample(d1, d2);

          if (z[j]) {
            phrases_.increment(p1, p0(p1), rng);
            phrases_.increment(p2, p0(p2), rng);
            gen_.increment(false, gen_p0_, rng);
            gen_.increment(false, gen_p0_, rng);
            prev = j + 1;
          } else {
            phrases_.increment(p1p2, p0(p1p2), rng);
            gen_.increment(false, gen_p0_, rng);
          }
        }
      }
    }
//    cerr << endl << endl << gen_ << endl << phrases_ << endl;
    cerr << gen_.prob(false, gen_p0_) << " " << gen_.prob(true, 1 - gen_p0_) << endl;
  }

  void WriteCdecGrammarForCurrentSample(ostream* os) const {
    CCRP<vector<int> >::const_iterator it = phrases_.begin();
    for (; it != phrases_.end(); ++it) {
      (*os) << "[X] ||| " << Join(' ', it->first) << " ||| "
                          << Join('_', it->first) << " ||| C=1 P=" 
                          << log(phrases_.prob(it->first, p0(it->first))) << endl;
    }
  }

  double OOVUnigramLogProb() const {
    vector<int> x(1,99999999);
    return log(phrases_.prob(x, p0(x)));
  }

  void ResampleHyperparameters(MT19937* rng) {
    phrases_.resample_hyperparameters(rng);
    gen_.resample_hyperparameters(rng);
    cerr << " d=" << phrases_.discount() << ",s=" << phrases_.strength();
  }

  CCRP<vector<int> > phrases_;
  CCRP<bool> gen_;
  vector<vector<bool> > z_;   // z_[i] is there a phrase boundary after the ith word
  const vector<vector<int> >& corpus_;
  const double uniform_word_;
  const double gen_p0_;
  const double p_end_; // in base length distribution, p of the end of a phrase
  const bool use_poisson_;
};


int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  shared_ptr<MT19937> prng;
  if (conf.count("random_seed"))
    prng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    prng.reset(new MT19937);
  MT19937& rng = *prng;

  vector<vector<int> > corpus;
  set<int> vocab;
  ReadCorpus(conf["input"].as<string>(), &corpus, &vocab);
  cerr << "Corpus size: " << corpus.size() << " sentences\n";
  cerr << "Vocabulary size: " << vocab.size() << " types\n";

  UniphraseLM ulm(corpus, vocab, conf);
  ulm.Sample(conf["samples"].as<unsigned>(), conf.count("no_hyperparameter_inference") == 0, &rng);
  cerr << "OOV unigram prob: " << ulm.OOVUnigramLogProb() << endl;

  for (int i = 0; i < corpus.size(); ++i)
//    SegCorpus::Write(corpus[i], shmmlm.z_[i], d);
 ;
  if (conf.count("write_cdec_grammar")) {
    string fname = conf["write_cdec_grammar"].as<string>();
    cerr << "Writing model to " << fname << " ...\n";
    WriteFile wf(fname);
    ulm.WriteCdecGrammarForCurrentSample(wf.stream());
  }

  if (conf.count("write_cdec_weights")) {
    string fname = conf["write_cdec_weights"].as<string>();
    cerr << "Writing weights to " << fname << " .\n";
    WriteFile wf(fname);
    ostream& os = *wf.stream();
    os << "# make C smaller to use more phrases\nP 1\nPassThrough " << ulm.OOVUnigramLogProb() << "\nC -3\n";
  }

  

  return 0;
}

