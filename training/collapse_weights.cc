char const* NOTES =
  "ZF_and_E means unnormalized scaled features.\n"
  "For grammars with one nonterminal: F_and_E is joint,\n"
  "F_given_E and E_given_F are conditional.\n"
  "TODO: group rules by root nonterminal and then normalize.\n";


#include <iostream>
#include <fstream>
#include <tr1/unordered_map>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/functional/hash.hpp>

#include "prob.h"
#include "filelib.h"
#include "trule.h"
#include "weights.h"

namespace po = boost::program_options;
using namespace std;

typedef std::tr1::unordered_map<vector<WordID>, prob_t, boost::hash<vector<WordID> > > MarginalMap;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("grammar,g", po::value<string>(), "Grammar file")
        ("weights,w", po::value<string>(), "Weights file")
    ("unnormalized,u", "Always include ZF_and_E unnormalized score (default: only if sum was >1)")
    ;
  po::options_description clo("Command line options");
  clo.add_options()
        ("config,c", po::value<string>(), "Configuration file")
        ("help,h", "Print this help message and exit");
  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts);
  dcmdline_options.add(opts).add(clo);

  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("config")) {
    const string cfg = (*conf)["config"].as<string>();
    cerr << "Configuration file: " << cfg << endl;
    ifstream config(cfg.c_str());
    po::store(po::parse_config_file(config, dconfig_options), *conf);
  }
  po::notify(*conf);

  if (conf->count("help") || !conf->count("grammar") || !conf->count("weights")) {
    cerr << dcmdline_options << endl;
    cerr << NOTES << endl;
    exit(1);
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const string wfile = conf["weights"].as<string>();
  const string gfile = conf["grammar"].as<string>();
  vector<weight_t> w;
  Weights::InitFromFile(wfile, &w);
  MarginalMap e_tots;
  MarginalMap f_tots;
  prob_t tot;
  {
    ReadFile rf(gfile);
    assert(*rf.stream());
    istream& in = *rf.stream();
    cerr << "Computing marginals...\n";
    int lc = 0;
    while(in) {
      string line;
      getline(in, line);
      ++lc;
      if (line.empty()) continue;
      TRule tr(line, true);
      if (tr.GetFeatureValues().empty())
        cerr << "Line " << lc << ": empty features - may introduce bias\n";
      prob_t prob;
      prob.logeq(tr.GetFeatureValues().dot(w));
      e_tots[tr.e_] += prob;
      f_tots[tr.f_] += prob;
      tot += prob;
    }
  }
  bool normalized = (fabs(log(tot)) < 0.001);
  cerr << "Total: " << tot << (normalized ? " [normalized]" : " [scaled]") << endl;
  ReadFile rf(gfile);
  istream&in = *rf.stream();
  while(in) {
    string line;
    getline(in, line);
    if (line.empty()) continue;
    TRule tr(line, true);
    const double lp = tr.GetFeatureValues().dot(w);
    if (isinf(lp)) { continue; }
    tr.scores_.clear();

    cout << tr.AsString() << " ||| F_and_E=" << lp - log(tot);
    if (!normalized || conf.count("unnormalized")) {
      cout << ";ZF_and_E=" << lp;
    }
    cout << ";F_given_E=" << lp - log(e_tots[tr.e_])
         << ";E_given_F=" << lp - log(f_tots[tr.f_]) << endl;
  }
  return 0;
}

