#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "filelib.h"
#include "fdict.h"
#include "weights.h"
#include "sparse_vector.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input_format,f",po::value<string>()->default_value("b64"),"Encoding of the input (b64 or text)")
        ("input,i",po::value<string>()->default_value("-"),"Read file from")
        ("output,o",po::value<string>()->default_value("-"),"Write weights to");
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

  if (conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

void WriteWeights(const SparseVector<double>& weights, ostream* out) {
  for (SparseVector<double>::const_iterator it = weights.begin();
       it != weights.end(); ++it) {
    (*out) << FD::Convert(it->first) << " " << it->second << endl;
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  const bool use_b64 = conf["input_format"].as<string>() == "b64";

  const string s_obj = "**OBJ**";
  // E-step
  ReadFile rf(conf["input"].as<string>());
  istream* in = rf.stream();
  assert(*in);
  WriteFile wf(conf["output"].as<string>());
  ostream* out = wf.stream();
  out->precision(17);
  while(*in) {
    string line;
    getline(*in, line);
    if (line.empty()) continue;
    int feat;
    double val;
    size_t i = line.find("\t");
    assert(i != string::npos);
    ++i;
    if (use_b64) {
      SparseVector<double> g;
      double obj;
      if (!B64::Decode(&obj, &g, &line[i], line.size() - i)) {
        cerr << "B64 decoder returned error, skipping!\n";
        continue;
      }
      WriteWeights(g, out);
    } else {       // text encoding - your counts will not be accurate!
      SparseVector<double> weights;
      while (i < line.size()) {
        size_t start = i;
        while (line[i] != '=' && i < line.size()) ++i;
        if (i == line.size()) { cerr << "FORMAT ERROR\n"; break; }
        string fname = line.substr(start, i - start);
        if (fname == s_obj) {
          feat = -1;
        } else {
          feat = FD::Convert(line.substr(start, i - start));
        }
        ++i;
        start = i;
        while (line[i] != ';' && i < line.size()) ++i;
        if (i - start == 0) continue;
        val = atof(line.substr(start, i - start).c_str());
        ++i;
        if (feat != -1) {
          weights.set_value(feat, val);
        }
      }
      WriteWeights(weights, out);
    }
  }

  return 0;
}
