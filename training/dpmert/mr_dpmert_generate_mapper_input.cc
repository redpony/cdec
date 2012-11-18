#include <iostream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "filelib.h"
#include "weights.h"
#include "line_optimizer.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("dev_set_size,s",po::value<unsigned>(),"[REQD] Development set size (# of parallel sentences)")
        ("forest_repository,r",po::value<string>(),"[REQD] Path to forest repository")
        ("weights,w",po::value<string>(),"[REQD] Current feature weights file")
        ("optimize_feature,o",po::value<vector<string> >(), "Feature to optimize (if none specified, all weights listed in the weights file will be optimized)")
        ("random_directions,d",po::value<unsigned int>()->default_value(20),"Number of random directions to run the line optimizer in")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  bool flag = false;
  if (conf->count("dev_set_size") == 0) {
    cerr << "Please specify the size of the development set using -d N\n";
    flag = true;
  }
  if (conf->count("weights") == 0) {
    cerr << "Please specify the starting-point weights using -w <weightfile.txt>\n";
    flag = true;
  }
  if (conf->count("forest_repository") == 0) {
    cerr << "Please specify the forest repository location using -r <DIR>\n";
    flag = true;
  }
  if (flag || conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

int main(int argc, char** argv) {
  RandomNumberGenerator<boost::mt19937> rng;
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  vector<string> features;
  SparseVector<weight_t> origin;
  vector<weight_t> w;
  Weights::InitFromFile(conf["weights"].as<string>(), &w, &features);
  Weights::InitSparseVector(w, &origin);
  const string forest_repository = conf["forest_repository"].as<string>();
  if (!DirectoryExists(forest_repository)) {
    cerr << "Forest repository directory " << forest_repository << " not found!\n";
    return 1;
  }
  if (conf.count("optimize_feature") > 0)
    features=conf["optimize_feature"].as<vector<string> >();
  vector<SparseVector<weight_t> > directions;
  vector<int> fids(features.size());
  for (unsigned i = 0; i < features.size(); ++i)
    fids[i] = FD::Convert(features[i]);
  LineOptimizer::CreateOptimizationDirections(
     fids,
     conf["random_directions"].as<unsigned int>(),
     &rng,
     &directions);
  unsigned dev_set_size = conf["dev_set_size"].as<unsigned>();
  for (unsigned i = 0; i < dev_set_size; ++i) {
    for (unsigned j = 0; j < directions.size(); ++j) {
      cout << forest_repository << '/' << i << ".json.gz " << i << ' ';
      print(cout, origin, "=", ";");
      cout << ' ';
      print(cout, directions[j], "=", ";");
      cout << endl;
    }
  }
  return 0;
}
