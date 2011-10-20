#include <iostream>
#include <vector>
#include <cassert>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "filelib.h"
#include "fdict.h"
#include "weights.h"

using namespace std;
namespace po = boost::program_options;

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("weights,w",po::value<string>(),"Input feature weights file")
        ("keys,k",po::value<string>(),"Keys file (list of features with dummy value at start)")
        ("cmph_perfect_hash_file,h",po::value<string>(),"cmph perfect hash function file");
  po::options_description clo("Command line options");
  clo.add_options()
        ("config", po::value<string>(), "Configuration file")
        ("help,?", "Print this help message and exit");
  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts);
  dcmdline_options.add(opts).add(clo);
  
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("config")) {
    ifstream config((*conf)["config"].as<string>().c_str());
    po::store(po::parse_config_file(config, dconfig_options), *conf);
  }
  po::notify(*conf);

  if (conf->count("help") || !conf->count("cmph_perfect_hash_file") || !conf->count("weights") || !conf->count("keys")) {
    cerr << "Generate a text format weights file. Options -w -k and -h are required.\n";
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf))
    return false;

  FD::EnableHash(conf["cmph_perfect_hash_file"].as<string>());

  // load weights
  vector<weight_t> weights;
  Weights::InitFromFile(conf["weights"].as<string>(), &weights);

  ReadFile rf(conf["keys"].as<string>());
  istream& in = *rf.stream();
  string key;
  size_t lc = 0;
  while(getline(in, key)) {
    ++lc;
    if (lc == 1) continue;
    assert(lc <= weights.size());
    cout << key << " " << weights[lc - 1] << endl;
  }

  return 0;
}

