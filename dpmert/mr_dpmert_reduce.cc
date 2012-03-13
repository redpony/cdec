#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "sparse_vector.h"
#include "error_surface.h"
#include "line_optimizer.h"
#include "b64tools.h"
#include "stringlib.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("evaluation_metric,m",po::value<string>(), "Evaluation metric (IBM_BLEU, etc.)")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  bool flag = conf->count("evaluation_metric") == 0;
  if (flag || conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const string evaluation_metric = conf["evaluation_metric"].as<string>();
  EvaluationMetric* metric = EvaluationMetric::Instance(evaluation_metric);
  LineOptimizer::ScoreType opt_type = LineOptimizer::MAXIMIZE_SCORE;
  if (metric->IsErrorMetric())
    opt_type = LineOptimizer::MINIMIZE_SCORE;

  vector<ErrorSurface> esv;
  string last_key, line, key, val;
  while(getline(cin, line)) {
    size_t ks = line.find("\t");
    assert(string::npos != ks);
    assert(ks > 2);
    key = line.substr(2, ks - 2);
    val = line.substr(ks + 1);
    if (key != last_key) {
      if (!last_key.empty()) {
	float score;
        double x = LineOptimizer::LineOptimize(metric, esv, opt_type, &score);
	cout << last_key << "|" << x << "|" << score << endl;
      }
      last_key.swap(key);
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
    esv.back().Deserialize(encoded);
  }
  if (!esv.empty()) {
    float score;
    double x = LineOptimizer::LineOptimize(metric, esv, opt_type, &score);
    cout << last_key << "|" << x << "|" << score << endl;
  }
  return 0;
}
