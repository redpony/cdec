#include <iostream>

#include "config.h"
#ifndef HAVE_EIGEN
  int main() { std::cerr << "Please rebuild with --with-eigen PATH\n"; return 1; }
#else

#include <cmath>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <Eigen/Dense>

#include "m.h"
#include "lattice.h"
#include "stringlib.h"
#include "filelib.h"
#include "tdict.h"

namespace po = boost::program_options;
using namespace std;

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("iterations,i",po::value<unsigned>()->default_value(5),"Number of iterations of training")
        ("diagonal_tension,T", po::value<double>()->default_value(4.0), "How sharp or flat around the diagonal is the alignment distribution (0 = uniform, >0 sharpens)")
        ("testset,x", po::value<string>(), "After training completes, compute the log likelihood of this set of sentence pairs under the learned model");
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
  const double diagonal_tension = conf["diagonal_tension"].as<double>();
  string testset;
  if (conf.count("testset")) testset = conf["testset"].as<string>();

  double tot_len_ratio = 0;
  double mean_srclen_multiplier = 0;
  vector<double> unnormed_a_i;
  for (int iter = 0; iter < ITERATIONS; ++iter) {
    cerr << "ITERATION " << (iter + 1) << endl;
    ReadFile rf(fname);
    istream& in = *rf.stream();
    double likelihood = 0;
    double denom = 0.0;
    int lc = 0;
    bool flag = false;
    string line;
    string ssrc, strg;
    while(true) {
      getline(in, line);
      if (!in) break;
      ++lc;
      if (lc % 1000 == 0) { cerr << '.'; flag = true; }
      if (lc %50000 == 0) { cerr << " [" << lc << "]\n" << flush; flag = false; }
      ParseTranslatorInput(line, &ssrc, &strg);
      Lattice src, trg;
      LatticeTools::ConvertTextToLattice(ssrc, &src);
      LatticeTools::ConvertTextToLattice(strg, &trg);
      if (src.size() == 0 || trg.size() == 0) {
        cerr << "Error: " << lc << "\n" << line << endl;
        assert(src.size() > 0);
        assert(trg.size() > 0);
      }
      if (src.size() > unnormed_a_i.size())
        unnormed_a_i.resize(src.size());
      if (iter == 0)
        tot_len_ratio += static_cast<double>(trg.size()) / static_cast<double>(src.size());
      denom += trg.size();
      vector<double> probs(src.size() + 1);
      bool first_al = true;  // used for write_alignments
      for (int j = 0; j < trg.size(); ++j) {
        const WordID& f_j = trg[j][0].label;
        double sum = 0;
        const double j_over_ts = double(j) / trg.size();
        double prob_a_i = 1.0 / src.size();
        double az = 0;
        for (int ta = 0; ta < src.size(); ++ta) {
          unnormed_a_i[ta] = exp(-fabs(double(ta) / src.size() - j_over_ts) * diagonal_tension);
          az += unnormed_a_i[ta];
        }
        for (int i = 1; i <= src.size(); ++i) {
          prob_a_i = unnormed_a_i[i-1] / az;
          probs[i] = 1; // tt.prob(src[i-1][0].label, f_j) * prob_a_i;
          sum += probs[i];
        }
      }
    }

    // log(e) = 1.0
    double base2_likelihood = likelihood / log(2);

    if (flag) { cerr << endl; }
    if (iter == 0) {
      mean_srclen_multiplier = tot_len_ratio / lc;
      cerr << "expected target length = source length * " << mean_srclen_multiplier << endl;
    }
    cerr << "  log_e likelihood: " << likelihood << endl;
    cerr << "  log_2 likelihood: " << base2_likelihood << endl;
    cerr << "   cross entropy: " << (-base2_likelihood / denom) << endl;
    cerr << "      perplexity: " << pow(2.0, -base2_likelihood / denom) << endl;
  }
  return 0;
}

#endif

