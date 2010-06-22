#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "sparse_vector.h"
#include "error_surface.h"
#include "line_optimizer.h"
#include "hg_io.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("loss_function,l",po::value<string>(), "Loss function being optimized")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  bool flag = conf->count("loss_function") == 0;
  if (flag || conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const string loss_function = conf["loss_function"].as<string>();
  ScoreType type = ScoreTypeFromString(loss_function);
  LineOptimizer::ScoreType opt_type = LineOptimizer::MAXIMIZE_SCORE;
  if (type == TER || type == AER) {
    opt_type = LineOptimizer::MINIMIZE_SCORE;
  }
  string last_key;
  vector<ErrorSurface> esv;
  while(cin) {
    string line;
    getline(cin, line);
    if (line.empty()) continue;
    size_t ks = line.find("\t");
    assert(string::npos != ks);
    assert(ks > 2);
    string key = line.substr(2, ks - 2);
    string val = line.substr(ks + 1);
    if (key != last_key) {
      if (!last_key.empty()) {
	float score;
        double x = LineOptimizer::LineOptimize(esv, opt_type, &score);
	cout << last_key << "|" << x << "|" << score << endl;
      }
      last_key = key;
      esv.clear();
    }
    if (val.size() % 4 != 0) {
      cerr << "B64 encoding error 1! Skipping.\n";
      continue;
    }
    string encoded(val.size() / 4 * 3, '\0');
    if (!B64::b64decode(reinterpret_cast<const unsigned char*>(&val[0]), val.size(), &encoded[0], encoded.size())) {
      cerr << "B64 encoding error 2! Skipping.\n";
      continue;
    }
    esv.push_back(ErrorSurface());
    esv.back().Deserialize(type, encoded);
  }
  if (!esv.empty()) {
    // cerr << "ESV=" << esv.size() << endl;
    // for (int i = 0; i < esv.size(); ++i) { cerr << esv[i].size() << endl; }
    float score;
    double x = LineOptimizer::LineOptimize(esv, opt_type, &score);
    cout << last_key << "|" << x << "|" << score << endl;
  }
  return 0;
}
