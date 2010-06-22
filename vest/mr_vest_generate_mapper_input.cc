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
        ("dev_set_size,s",po::value<unsigned int>(),"[REQD] Development set size (# of parallel sentences)")
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
  Weights weights;
  vector<string> features;
  weights.InitFromFile(conf["weights"].as<string>(), &features);
  const string forest_repository = conf["forest_repository"].as<string>();
  assert(DirectoryExists(forest_repository));
  SparseVector<double> origin;
  weights.InitSparseVector(&origin);
  if (conf.count("optimize_feature") > 0)
    features=conf["optimize_feature"].as<vector<string> >();
  vector<SparseVector<double> > axes;
  vector<int> fids(features.size());
  for (int i = 0; i < features.size(); ++i)
    fids[i] = FD::Convert(features[i]);
  LineOptimizer::CreateOptimizationDirections(
     fids,
     conf["random_directions"].as<unsigned int>(),
     &rng,
     &axes);
  int dev_set_size = conf["dev_set_size"].as<unsigned int>();
  for (int i = 0; i < dev_set_size; ++i)
    for (int j = 0; j < axes.size(); ++j)
      cout << forest_repository << '/' << i << ".json.gz " << i << ' ' << origin << ' ' << axes[j] << endl;
  return 0;
}
