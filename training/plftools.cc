char const* NOTES =
  "Process (PLF format) lattice: sharpen distribution, nbest, graphviz, push weights\n"
  ;

#include <iostream>
#include <fstream>
#include <vector>

#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "filelib.h"
#include "tdict.h"
#include "prob.h"
#include "hg.h"
#include "hg_io.h"
#include "viterbi.h"
#include "kbest.h"

namespace po = boost::program_options;
using namespace std;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input,i", po::value<string>(), "REQ. Lattice input file (PLF), - for STDIN")
        ("prior_scale,p", po::value<double>()->default_value(1.0), "Scale path probabilities by this amount < 1 flattens, > 1 sharpens")
        ("weight,w", po::value<vector<double> >(), "Weight(s) for arc features")
	("output,o", po::value<string>()->default_value("plf"), "Output format (text, plf)")
	("command,c", po::value<string>()->default_value("push"), "Operation to perform: push, graphviz, 1best, 2best ...")
        ("help,h", "Print this help message and exit");
  po::options_description clo("Command line options");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);

  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  po::notify(*conf);

  if (conf->count("help") || conf->count("input") == 0) {
    cerr << dcmdline_options << endl << NOTES << endl;
    exit(1);
  }
}

int main(int argc, char **argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  string infile = conf["input"].as<string>();
  ReadFile rf(infile);
  istream* in = rf.stream();
  assert(*in);
  SparseVector<double> wts;
  vector<double> wv;
  if (conf.count("weight") > 0) wv = conf["weight"].as<vector<double> >();
  if (wv.empty()) wv.push_back(1.0);
  for (int i = 0; i < wv.size(); ++i) {
    const string fname = "Feature_" + boost::lexical_cast<string>(i);
    cerr << "[INFO] Arc weight " << (i+1) << " = " << wv[i] << endl;
    wts.set_value(FD::Convert(fname), wv[i]);
  }
  const string cmd = conf["command"].as<string>();
  const bool push_weights = cmd == "push";
  const bool output_plf = cmd == "plf";
  const bool graphviz = cmd == "graphviz";
  const bool kbest = cmd.rfind("best") == (cmd.size() - 4) && cmd.size() > 4;
  int k = 1;
  if (kbest) {
    k = boost::lexical_cast<int>(cmd.substr(0, cmd.size() - 4));
    cerr << "KBEST = " << k << endl;
  }
  const double scale = conf["prior_scale"].as<double>();
  int lc = 0;
  while(*in) {
    ++lc;
    string plf;
    getline(*in, plf);
    if (plf.empty()) continue;
    Hypergraph hg;
    HypergraphIO::ReadFromPLF(plf, &hg);
    hg.Reweight(wts);
    if (graphviz) hg.PrintGraphviz();
    if (push_weights) hg.PushWeightsToSource(scale);
    if (output_plf) {
      cout << HypergraphIO::AsPLF(hg) << endl;
    } else {
      KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg, k);
      for (int i = 0; i < k; ++i) {
        const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
          kbest.LazyKthBest(hg.nodes_.size() - 1, i);
        if (!d) break;
        cout << lc << " ||| " << TD::GetString(d->yield) << " ||| " << d->score << endl;
      }
    }
  }
  return 0;
}

