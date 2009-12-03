#include <iostream>
#include <string>
#include <sstream>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "hg.h"
#include "hg_io.h"
#include "filelib.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("dev_set_size,s",po::value<unsigned int>(),"[REQD] Development set size (# of parallel sentences)")
        ("forest_repository,r",po::value<string>(),"[REQD] Path to forest repository")
        ("new_forest_repository,n",po::value<string>(),"[REQD] Path to new forest repository")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  bool flag = false;
  if (conf->count("dev_set_size") == 0) {
    cerr << "Please specify the size of the development set using -d N\n";
    flag = true;
  }
  if (conf->count("new_forest_repository") == 0) {
    cerr << "Please specify the starting-point weights using -n PATH\n";
    flag = true;
  }
  if (conf->count("forest_repository") == 0) {
    cerr << "Please specify the forest repository location using -r PATH\n";
    flag = true;
  }
  if (flag || conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const int size = conf["dev_set_size"].as<unsigned int>();
  const string repo = conf["forest_repository"].as<string>();
  const string new_repo = conf["new_forest_repository"].as<string>();
  for (int i = 0; i < size; ++i) {
    ostringstream sfin, sfout;
    sfin << new_repo << '/' << i << ".json.gz";
    sfout << repo << '/' << i << ".json.gz";
    const string fin = sfin.str();
    const string fout = sfout.str();
    Hypergraph existing_hg;
    cerr << "Processing " << fin << endl;
    assert(FileExists(fin));
    if (FileExists(fout)) {
      ReadFile rf(fout);
      assert(HypergraphIO::ReadFromJSON(rf.stream(), &existing_hg));
    }
    Hypergraph new_hg;
    if (true) {
      ReadFile rf(fin);
      assert(HypergraphIO::ReadFromJSON(rf.stream(), &new_hg));
    }
    existing_hg.Union(new_hg);
    WriteFile wf(fout);
    assert(HypergraphIO::WriteToJSON(existing_hg, false, wf.stream()));
  }
  return 0;
}
