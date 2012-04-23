#include "arc_factored.h"

#include <vector>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "timing_stats.h"
#include "arc_ff.h"
#include "dep_training.h"
#include "stringlib.h"
#include "filelib.h"
#include "tdict.h"
#include "weights.h"
#include "rst.h"
#include "global_ff.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  string cfg_file;
  opts.add_options()
        ("input,i",po::value<string>()->default_value("-"), "File containing test data (jsent format)")
        ("q_weights,q",po::value<string>(), "Arc-factored weights for proposal distribution (mandatory)")
        ("p_weights,p",po::value<string>(), "Weights for target distribution (optional)")
        ("samples,n",po::value<unsigned>()->default_value(1000), "Number of samples");
  po::options_description clo("Command line options");
  clo.add_options()
        ("config,c", po::value<string>(&cfg_file), "Configuration file")
        ("help,?", "Print this help message and exit");

  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts);
  dcmdline_options.add(dconfig_options).add(clo);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (cfg_file.size() > 0) {
    ReadFile rf(cfg_file);
    po::store(po::parse_config_file(*rf.stream(), dconfig_options), *conf);
  }
  if (conf->count("help") || conf->count("q_weights") == 0) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  vector<weight_t> qweights, pweights;
  Weights::InitFromFile(conf["q_weights"].as<string>(), &qweights);
  if (conf.count("p_weights"))
    Weights::InitFromFile(conf["p_weights"].as<string>(), &pweights);
  const bool global = pweights.size() > 0;
  ArcFeatureFunctions ffs;
  GlobalFeatureFunctions gff;
  ReadFile rf(conf["input"].as<string>());
  istream* in = rf.stream();
  TrainingInstance sent;
  MT19937 rng;
  int samples = conf["samples"].as<unsigned>();
  int totroot = 0, root_right = 0, tot = 0, cor = 0;
  while(TrainingInstance::ReadInstance(in, &sent)) {
    ffs.PrepareForInput(sent.ts);
    if (global) gff.PrepareForInput(sent.ts);
    ArcFactoredForest forest(sent.ts.pos.size());
    forest.ExtractFeatures(sent.ts, ffs);
    forest.Reweight(qweights);
    TreeSampler ts(forest);
    double best_score = -numeric_limits<double>::infinity();
    EdgeSubset best_tree;
    for (int n = 0; n < samples; ++n) {
      EdgeSubset tree;
      ts.SampleRandomSpanningTree(&tree, &rng);
      SparseVector<double> qfeats, gfeats;
      tree.ExtractFeatures(sent.ts, ffs, &qfeats);
      double score = 0;
      if (global) {
        gff.Features(sent.ts, tree, &gfeats);
        score = (qfeats + gfeats).dot(pweights);
      } else {
        score = qfeats.dot(qweights);
      }
      if (score > best_score) {
        best_tree = tree;
        best_score = score;
      }
    }
    cerr << "BEST SCORE: " << best_score << endl;
    cout << best_tree << endl;
    const bool sent_has_ref = sent.tree.h_m_pairs.size() > 0;
    if (sent_has_ref) {
      map<pair<short,short>, bool> ref;
      for (int i = 0; i < sent.tree.h_m_pairs.size(); ++i)
        ref[sent.tree.h_m_pairs[i]] = true;
      int ref_root = sent.tree.roots.front();
      if (ref_root == best_tree.roots.front()) { ++root_right; }
      ++totroot;
      for (int i = 0; i < best_tree.h_m_pairs.size(); ++i) {
        if (ref[best_tree.h_m_pairs[i]]) {
          ++cor;
        }
        ++tot;
      }
    }
  }
  cerr << "F = " << (double(cor + root_right) / (tot + totroot)) << endl;
  return 0;
}

