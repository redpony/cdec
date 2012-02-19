#include <iostream>

#include "config.h"
#ifndef HAVE_EIGEN
  int main() { std::cerr << "Please rebuild with --with-eigen PATH\n"; return 1; }
#else

#include <cmath>
#include <set>

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

#define kDIMENSIONS 10
typedef Eigen::Matrix<float, kDIMENSIONS, 1> RVector;
typedef Eigen::Matrix<float, 1, kDIMENSIONS> RTVector;
typedef Eigen::Matrix<float, kDIMENSIONS, kDIMENSIONS> TMatrix;
vector<RVector> r_src, r_trg;

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input,i",po::value<string>(),"Input file")
        ("iterations,I",po::value<unsigned>()->default_value(1000),"Number of iterations of training")
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
    cerr << "Usage " << argv[0] << " [OPTIONS] -i corpus.fr-en\n";
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf)) return 1;
  const string fname = conf["input"].as<string>();
  const int ITERATIONS = conf["iterations"].as<unsigned>();
  const double diagonal_tension = conf["diagonal_tension"].as<double>();
  if (diagonal_tension < 0.0) {
    cerr << "Invalid value for diagonal_tension: must be >= 0\n";
    return 1;
  }
  string testset;
  if (conf.count("testset")) testset = conf["testset"].as<string>();

  int lc = 0;
  vector<double> unnormed_a_i;
  string line;
  string ssrc, strg;
  bool flag = false;
  Lattice src, trg;
  set<WordID> vocab_e;
  { // read through corpus, initialize int map, check lines are good
    cerr << "INITIAL READ OF " << fname << endl;
    ReadFile rf(fname);
    istream& in = *rf.stream();
    while(getline(in, line)) {
      ++lc;
      if (lc % 1000 == 0) { cerr << '.'; flag = true; }
      if (lc %50000 == 0) { cerr << " [" << lc << "]\n" << flush; flag = false; }
      ParseTranslatorInput(line, &ssrc, &strg);
      LatticeTools::ConvertTextToLattice(ssrc, &src);
      LatticeTools::ConvertTextToLattice(strg, &trg);
      if (src.size() == 0 || trg.size() == 0) {
        cerr << "Error: " << lc << "\n" << line << endl;
        assert(src.size() > 0);
        assert(trg.size() > 0);
      }
      if (src.size() > unnormed_a_i.size())
        unnormed_a_i.resize(src.size());
      for (unsigned i = 0; i < trg.size(); ++i) {
        assert(trg[i].size() == 1);
        vocab_e.insert(trg[i][0].label);
      }
    }
  }
  if (flag) cerr << endl;

  // do optimization
  for (int iter = 0; iter < ITERATIONS; ++iter) {
    cerr << "ITERATION " << (iter + 1) << endl;
    ReadFile rf(fname);
    istream& in = *rf.stream();
    double likelihood = 0;
    double denom = 0.0;
    lc = 0;
    flag = false;
    while(true) {
      getline(in, line);
      if (!in) break;
      ++lc;
      if (lc % 1000 == 0) { cerr << '.'; flag = true; }
      if (lc %50000 == 0) { cerr << " [" << lc << "]\n" << flush; flag = false; }
      ParseTranslatorInput(line, &ssrc, &strg);
      LatticeTools::ConvertTextToLattice(ssrc, &src);
      LatticeTools::ConvertTextToLattice(strg, &trg);
      denom += trg.size();
      vector<double> probs(src.size() + 1);
      for (int j = 0; j < trg.size(); ++j) {
        const WordID& f_j = trg[j][0].label;
        double sum = 0;
        const double j_over_ts = double(j) / trg.size();
        double az = 0;
        for (int ta = 0; ta < src.size(); ++ta) {
          unnormed_a_i[ta] = exp(-fabs(double(ta) / src.size() - j_over_ts) * diagonal_tension);
          az += unnormed_a_i[ta];
        }
        for (int i = 1; i <= src.size(); ++i) {
          const double prob_a_i = unnormed_a_i[i-1] / az;
          // TODO
          probs[i] = 1; // tt.prob(src[i-1][0].label, f_j) * prob_a_i;
          sum += probs[i];
        }
      }
    }
    if (flag) { cerr << endl; }

    const double base2_likelihood = likelihood / log(2);
    cerr << "  log_e likelihood: " << likelihood << endl;
    cerr << "  log_2 likelihood: " << base2_likelihood << endl;
    cerr << "   cross entropy: " << (-base2_likelihood / denom) << endl;
    cerr << "      perplexity: " << pow(2.0, -base2_likelihood / denom) << endl;
  }
  return 0;
}

#endif

