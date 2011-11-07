#include <fstream>
#include <iostream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "lm/model.hh"
#include "lm/enumerate_vocab.hh"

namespace po = boost::program_options;
using namespace std;

lm::ngram::ProbingModel* ngram;
struct GetVocab : public lm::EnumerateVocab {
  GetVocab(vector<lm::WordIndex>* out) : out_(out) { }
  void Add(lm::WordIndex index, const StringPiece &str) {
    out_->push_back(index);
  }
  vector<lm::WordIndex>* out_;
};

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("model,m",po::value<string>(),"n-gram language model file (KLM)");
  po::options_description clo("Command line options");
  clo.add_options()
        ("config", po::value<string>(), "Configuration file")
        ("help,h", "Print this help message and exit");
  po::options_description dconfig_options, dcmdline_options;
  po::positional_options_description p;
  p.add("grammar", -1);
  
  dconfig_options.add(opts);
  dcmdline_options.add(opts).add(clo);

  po::store(po::command_line_parser(argc, argv).options(dcmdline_options).positional(p).run(), *conf);
  if (conf->count("config")) {
    ifstream config((*conf)["config"].as<string>().c_str());
    po::store(po::parse_config_file(config, dconfig_options), *conf);
  }
  po::notify(*conf);

  if (conf->count("help")) {
    cerr << "Usage " << argv[0] << " [OPTIONS]\n";
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

template <class Model> double BlanketProb(const vector<lm::WordIndex>& sentence, const lm::WordIndex word, const int subst_pos, const Model &model) {
  typename Model::State state, out;
  lm::FullScoreReturn ret;
  double total = 0;
  state = model.NullContextState();

  const int begin = max(subst_pos - model.Order() + 1, 0);
  const int end = min(subst_pos + model.Order(), (int)sentence.size());
  int lookups = 0;
  bool have_full_context = false;
  for (int i = begin; i < end; ++i) {
    if (i == 0) {
      state = model.BeginSentenceState();
      have_full_context = true;
    } else {
      lookups++;
      if (lookups == model.Order()) { have_full_context = true; }
      ret = model.FullScore(state, (subst_pos == i ? word : sentence[i]), out);
      if (have_full_context) { total += ret.prob; }
      state = out;
    }
  }
  return total;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf)) return 1;
  lm::ngram::Config kconf;
  vector<lm::WordIndex> vocab;
  GetVocab gv(&vocab);
  kconf.enumerate_vocab = &gv; 
  ngram = new lm::ngram::ProbingModel(conf["model"].as<string>().c_str(), kconf);
  cerr << "Loaded " << (int)ngram->Order() << "-gram KenLM (vocab size=" << vocab.size() << ")\n";
  vector<int> exclude(vocab.size(), 0);
  exclude[0] = 1; // exclude OOVs

  double prob_sum = 0;
  int counter = 0;
  int rank_error = 0;
  string line;
  while (getline(cin, line)) {
    stringstream line_stream(line);
    vector<string> tokens;
    tokens.push_back("<s>");
    string token;
    while (line_stream >> token)
      tokens.push_back(token);
    tokens.push_back("</s>");

    vector<lm::WordIndex> sentence(tokens.size());
    for (int i = 0; i < tokens.size(); ++i)
      sentence[i] = ngram->GetVocabulary().Index(tokens[i]);
    exclude[sentence[0]] = 1;
    exclude[sentence.back()] = 1;
    for (int i = 1; i < tokens.size()-1; ++i) {
      cerr << tokens[i] << endl;
      ++counter;
      lm::WordIndex gold = sentence[i];
      double blanket_prob = BlanketProb<lm::ngram::ProbingModel>(sentence, gold, i, *ngram);
      double z = 0;
      for (int v = 0; v < vocab.size(); ++v) {
        if (exclude[v]) continue;
        double lp = BlanketProb<lm::ngram::ProbingModel>(sentence, v, i, *ngram);
        if (lp > blanket_prob) ++rank_error;
        z += pow(10.0, lp);
      }
      double post_prob = blanket_prob - log10(z);
      cerr << "  " << post_prob << endl;
      prob_sum -= post_prob;
    }
  }
  cerr << "perplexity=" << pow(10,prob_sum/(double)counter) << endl;
  cerr << "Rank error=" << rank_error/(double)counter << endl;

  return 0;
}

