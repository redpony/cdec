#include <iostream>
#include <cmath>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "lattice.h"
#include "stringlib.h"
#include "filelib.h"
#include "ttables.h"
#include "tdict.h"
#include "em_utils.h"

namespace po = boost::program_options;
using namespace std;

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("iterations,i",po::value<unsigned>()->default_value(5),"Number of iterations of EM training")
        ("beam_threshold,t",po::value<double>()->default_value(-4),"log_10 of beam threshold (-10000 to include everything, 0 max)")
        ("no_null_word,N","Do not generate from the null token")
        ("variational_bayes,v","Add a symmetric Dirichlet prior and infer VB estimate of weights")
        ("alpha,a", po::value<double>()->default_value(0.01), "Hyperparameter for optional Dirichlet prior")
        ("no_add_viterbi,V","Do not add Viterbi alignment points (may generate a grammar where some training sentence pairs are unreachable)");
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

  if (argc < 2 || conf->count("help")) {
    cerr << "Usage " << argv[0] << " [OPTIONS] corpus.fr-en\n";
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf)) return 1;
  const string fname = argv[argc - 1];
  const int ITERATIONS = conf["iterations"].as<unsigned>();
  const double BEAM_THRESHOLD = pow(10.0, conf["beam_threshold"].as<double>());
  const bool use_null = (conf.count("no_null_word") == 0);
  const WordID kNULL = TD::Convert("<eps>");
  const bool add_viterbi = (conf.count("no_add_viterbi") == 0);
  const bool variational_bayes = (conf.count("variational_bayes") > 0);
  const double alpha = conf["alpha"].as<double>();
  if (variational_bayes && alpha <= 0.0) {
    cerr << "--alpha must be > 0\n";
    return 1;
  }

  TTable tt;
  TTable::Word2Word2Double was_viterbi;
  for (int iter = 0; iter < ITERATIONS; ++iter) {
    const bool final_iteration = (iter == (ITERATIONS - 1));
    cerr << "ITERATION " << (iter + 1) << (final_iteration ? " (FINAL)" : "") << endl;
    ReadFile rf(fname);
    istream& in = *rf.stream();
    double likelihood = 0;
    double denom = 0.0;
    int lc = 0;
    bool flag = false;
    string line;
    while(true) {
      getline(in, line);
      if (!in) break;
      ++lc;
      if (lc % 1000 == 0) { cerr << '.'; flag = true; }
      if (lc %50000 == 0) { cerr << " [" << lc << "]\n" << flush; flag = false; }
      string ssrc, strg;
      ParseTranslatorInput(line, &ssrc, &strg);
      Lattice src, trg;
      LatticeTools::ConvertTextToLattice(ssrc, &src);
      LatticeTools::ConvertTextToLattice(strg, &trg);
      if (src.size() == 0 || trg.size() == 0) {
        cerr << "Error: " << lc << "\n" << line << endl;
        assert(src.size() > 0);
        assert(trg.size() > 0);
      }
      denom += trg.size();
      vector<double> probs(src.size() + 1);
      const double src_logprob = -log(src.size() + 1);
      for (int j = 0; j < trg.size(); ++j) {
        const WordID& f_j = trg[j][0].label;
        double sum = 0;
        if (use_null) {
          probs[0] = tt.prob(kNULL, f_j);
          sum += probs[0];
        }
        for (int i = 1; i <= src.size(); ++i) {
          probs[i] = tt.prob(src[i-1][0].label, f_j);
          sum += probs[i];
        }
        if (final_iteration) {
          if (add_viterbi) {
            WordID max_i = 0;
            double max_p = -1;
            if (use_null) {
              max_i = kNULL;
              max_p = probs[0];
            }
            for (int i = 1; i <= src.size(); ++i) {
              if (probs[i] > max_p) {
                max_p = probs[i];
                max_i = src[i-1][0].label;
              }
            }
            was_viterbi[max_i][f_j] = 1.0;
          }
        } else {
          if (use_null)
            tt.Increment(kNULL, f_j, probs[0] / sum);
          for (int i = 1; i <= src.size(); ++i)
            tt.Increment(src[i-1][0].label, f_j, probs[i] / sum);
        }
        likelihood += log(sum) + src_logprob;
      }
    }

    // log(e) = 1.0
    double base2_likelihood = likelihood / log(2);

    if (flag) { cerr << endl; }
    cerr << "  log_e likelihood: " << likelihood << endl;
    cerr << "  log_2 likelihood: " << base2_likelihood << endl;
    cerr << "   cross entropy: " << (-base2_likelihood / denom) << endl;
    cerr << "      perplexity: " << pow(2.0, -base2_likelihood / denom) << endl;
    if (!final_iteration) {
      if (variational_bayes)
        tt.NormalizeVB(alpha);
      else
        tt.Normalize();
    }
  }
  for (TTable::Word2Word2Double::iterator ei = tt.ttable.begin(); ei != tt.ttable.end(); ++ei) {
    const TTable::Word2Double& cpd = ei->second;
    const TTable::Word2Double& vit = was_viterbi[ei->first];
    const string& esym = TD::Convert(ei->first);
    double max_p = -1;
    for (TTable::Word2Double::const_iterator fi = cpd.begin(); fi != cpd.end(); ++fi)
      if (fi->second > max_p) max_p = fi->second;
    const double threshold = max_p * BEAM_THRESHOLD;
    for (TTable::Word2Double::const_iterator fi = cpd.begin(); fi != cpd.end(); ++fi) {
      if (fi->second > threshold || (vit.count(fi->first) > 0)) {
        cout << esym << ' ' << TD::Convert(fi->first) << ' ' << log(fi->second) << endl;
      }
    } 
  }
  return 0;
}

