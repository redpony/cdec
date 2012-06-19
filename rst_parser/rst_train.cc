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
        ("training_data,t",po::value<string>()->default_value("-"), "File containing training data (jsent format)")
        ("q_weights,q",po::value<string>(), "Arc-factored weights for proposal distribution")
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
  if (conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  vector<weight_t> qweights(FD::NumFeats(), 0.0);
  Weights::InitFromFile(conf["q_weights"].as<string>(), &qweights);
  vector<TrainingInstance> corpus;
  ArcFeatureFunctions ffs;
  GlobalFeatureFunctions gff;
  TrainingInstance::ReadTrainingCorpus(conf["training_data"].as<string>(), &corpus);
  vector<ArcFactoredForest> forests(corpus.size());
  vector<prob_t> zs(corpus.size());
  SparseVector<double> empirical;
  bool flag = false;
  for (int i = 0; i < corpus.size(); ++i) {
    TrainingInstance& cur = corpus[i];
    if ((i+1) % 10 == 0) { cerr << '.' << flush; flag = true; }
    if ((i+1) % 400 == 0) { cerr << " [" << (i+1) << "]\n"; flag = false; }
    SparseVector<weight_t> efmap;
    ffs.PrepareForInput(cur.ts);
    gff.PrepareForInput(cur.ts);
    for (int j = 0; j < cur.tree.h_m_pairs.size(); ++j) {
      efmap.clear();
      ffs.EdgeFeatures(cur.ts, cur.tree.h_m_pairs[j].first,
                       cur.tree.h_m_pairs[j].second,
                       &efmap);
      cur.features += efmap;
    }
    for (int j = 0; j < cur.tree.roots.size(); ++j) {
      efmap.clear();
      ffs.EdgeFeatures(cur.ts, -1, cur.tree.roots[j], &efmap);
      cur.features += efmap;
    }
    efmap.clear();
    gff.Features(cur.ts, cur.tree, &efmap);
    cur.features += efmap;
    empirical += cur.features;
    forests[i].resize(cur.ts.words.size());
    forests[i].ExtractFeatures(cur.ts, ffs);
    forests[i].Reweight(qweights);
    forests[i].EdgeMarginals(&zs[i]);
    zs[i] = prob_t::One() / zs[i];
    // cerr << zs[i] << endl;
    forests[i].Reweight(qweights);    // EdgeMarginals overwrites edge_prob
  }
  if (flag) cerr << endl;
  MT19937 rng;
  SparseVector<double> model_exp;
  SparseVector<double> weights;
  Weights::InitSparseVector(qweights, &weights);
  int samples = conf["samples"].as<unsigned>();
  for (int i = 0; i < corpus.size(); ++i) {
#if 0
    forests[i].EdgeMarginals();
    model_exp.clear();
    for (int h = -1; h < num_words; ++h) {
      for (int m = 0; m < num_words; ++m) {
        if (h == m) continue;
        const ArcFactoredForest::Edge& edge = forests[i](h,m);
        const SparseVector<weight_t>& fmap = edge.features;
        double prob = edge.edge_prob.as_float();
        model_exp += fmap * prob;
      }
    }
    cerr << "TRUE EXP: " << model_exp << endl;
    forests[i].Reweight(weights);
#endif

    TreeSampler ts(forests[i]);
    prob_t zhat = prob_t::Zero();
    SparseVector<prob_t> sampled_exp;
    for (int n = 0; n < samples; ++n) {
      EdgeSubset tree;
      ts.SampleRandomSpanningTree(&tree, &rng);
      SparseVector<double> qfeats, gfeats;
      tree.ExtractFeatures(corpus[i].ts, ffs, &qfeats);
      prob_t u; u.logeq(qfeats.dot(qweights));
      const prob_t q = u / zs[i];  // proposal mass
      gff.Features(corpus[i].ts, tree, &gfeats);
      SparseVector<double> tot_feats = qfeats + gfeats;
      u.logeq(tot_feats.dot(weights));
      prob_t w = u / q;
      zhat += w;
      for (SparseVector<double>::iterator it = tot_feats.begin(); it != tot_feats.end(); ++it)
        sampled_exp.add_value(it->first, w * prob_t(it->second));
    }
    sampled_exp /= zhat;
    SparseVector<double> tot_m;
    for (SparseVector<prob_t>::iterator it = sampled_exp.begin(); it != sampled_exp.end(); ++it)
      tot_m.add_value(it->first, it->second.as_float());
    //cerr << "DIFF: " << (tot_m - corpus[i].features) << endl;
    const double eta = 0.03;
    weights -= (tot_m - corpus[i].features) * eta;
  }
  cerr << "WEIGHTS.\n";
  cerr << weights << endl;
  return 0;
}

