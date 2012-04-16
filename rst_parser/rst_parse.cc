#include "arc_factored.h"

#include <vector>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "timing_stats.h"
#include "arc_ff.h"
#include "arc_ff_factory.h"
#include "dep_training.h"
#include "stringlib.h"
#include "filelib.h"
#include "tdict.h"
#include "weights.h"
#include "rst.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  string cfg_file;
  opts.add_options()
        ("training_data,t",po::value<string>()->default_value("-"), "File containing training data (jsent format)")
        ("feature_function,F",po::value<vector<string> >()->composing(), "feature function (multiple permitted)")
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
  ArcFactoredForest af(5);
  ArcFFRegistry reg;
  reg.Register("DistancePenalty", new ArcFFFactory<DistancePenalty>);
  vector<TrainingInstance> corpus;
  vector<boost::shared_ptr<ArcFeatureFunction> > ffs;
  ffs.push_back(boost::shared_ptr<ArcFeatureFunction>(new DistancePenalty("")));
  TrainingInstance::ReadTraining(conf["training_data"].as<string>(), &corpus);
  vector<ArcFactoredForest> forests(corpus.size());
  SparseVector<double> empirical;
  bool flag = false;
  for (int i = 0; i < corpus.size(); ++i) {
    TrainingInstance& cur = corpus[i];
    if ((i+1) % 10 == 0) { cerr << '.' << flush; flag = true; }
    if ((i+1) % 400 == 0) { cerr << " [" << (i+1) << "]\n"; flag = false; }
    for (int fi = 0; fi < ffs.size(); ++fi) {
      ArcFeatureFunction& ff = *ffs[fi];
      ff.PrepareForInput(cur.ts);
      SparseVector<weight_t> efmap;
      for (int j = 0; j < cur.tree.h_m_pairs.size(); ++j) {
        efmap.clear();
        ff.EgdeFeatures(cur.ts, cur.tree.h_m_pairs[j].first,
                        cur.tree.h_m_pairs[j].second,
                        &efmap);
        cur.features += efmap;
      }
      for (int j = 0; j < cur.tree.roots.size(); ++j) {
        efmap.clear();
        ff.EgdeFeatures(cur.ts, -1, cur.tree.roots[j], &efmap);
        cur.features += efmap;
      }
    }
    empirical += cur.features;
    forests[i].resize(cur.ts.words.size());
    forests[i].ExtractFeatures(cur.ts, ffs);
  }
  if (flag) cerr << endl;
  vector<weight_t> weights(FD::NumFeats(), 0.0);
  Weights::InitFromFile(conf["q_weights"].as<string>(), &weights);
  MT19937 rng;
  SparseVector<double> model_exp;
  SparseVector<double> sampled_exp;
  int samples = conf["samples"].as<unsigned>();
  for (int i = 0; i < corpus.size(); ++i) {
    const int num_words = corpus[i].ts.words.size();
    forests[i].Reweight(weights);
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
    //cerr << "TRUE EXP: " << model_exp << endl;

    forests[i].Reweight(weights);
    TreeSampler ts(forests[i]);
    sampled_exp.clear();
      //ostringstream os; os << "Samples_" << samples;
      //Timer t(os.str());
      for (int n = 0; n < samples; ++n) {
        EdgeSubset tree;
        ts.SampleRandomSpanningTree(&tree, &rng);
        SparseVector<double> feats;
        tree.ExtractFeatures(corpus[i].ts, ffs, &feats);
        sampled_exp += feats;
      }
      sampled_exp /= samples;
      cerr << "L2 norm of diff @ " << samples << " samples: " << (model_exp - sampled_exp).l2norm() << endl;
  }
  return 0;
}

