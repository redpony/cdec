#include "arc_factored.h"

#include <vector>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "arc_ff.h"
#include "stringlib.h"
#include "filelib.h"
#include "tdict.h"
#include "dep_training.h"
#include "optimize.h"
#include "weights.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  string cfg_file;
  opts.add_options()
        ("training_data,t",po::value<string>()->default_value("-"), "File containing training data (jsent format)")
        ("weights,w",po::value<string>(), "Optional starting weights")
        ("output_every_i_iterations,I",po::value<unsigned>()->default_value(1), "Write weights every I iterations")
        ("regularization_strength,C",po::value<double>()->default_value(1.0), "Regularization strength")
        ("correction_buffers,m", po::value<int>()->default_value(10), "LBFGS correction buffers");
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

void AddFeatures(double prob, const SparseVector<double>& fmap, vector<double>* g) {
  for (SparseVector<double>::const_iterator it = fmap.begin(); it != fmap.end(); ++it)
    (*g)[it->first] += it->second * prob;
}

double ApplyRegularizationTerms(const double C,
                                const vector<double>& weights,
                                vector<double>* g) {
  assert(weights.size() == g->size());
  double reg = 0;
  for (size_t i = 0; i < weights.size(); ++i) {
//    const double prev_w_i = (i < prev_weights.size() ? prev_weights[i] : 0.0);
    const double& w_i = weights[i];
    double& g_i = (*g)[i];
    reg += C * w_i * w_i;
    g_i += 2 * C * w_i;

//    reg += T * (w_i - prev_w_i) * (w_i - prev_w_i);
//    g_i += 2 * T * (w_i - prev_w_i);
  }
  return reg;
}

int main(int argc, char** argv) {
  int rank = 0;
  int size = 1;
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  ArcFeatureFunctions ffs;
  vector<TrainingInstance> corpus;
  TrainingInstance::ReadTrainingCorpus(conf["training_data"].as<string>(), &corpus, rank, size);
  vector<ArcFactoredForest> forests(corpus.size());
  SparseVector<double> empirical;
  bool flag = false;
  for (int i = 0; i < corpus.size(); ++i) {
    TrainingInstance& cur = corpus[i];
    if (rank == 0 && (i+1) % 10 == 0) { cerr << '.' << flush; flag = true; }
    if (rank == 0 && (i+1) % 400 == 0) { cerr << " [" << (i+1) << "]\n"; flag = false; }
    ffs.PrepareForInput(cur.ts);
    SparseVector<weight_t> efmap;
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
    empirical += cur.features;
    forests[i].resize(cur.ts.words.size());
    forests[i].ExtractFeatures(cur.ts, ffs);
  }
  if (flag) cerr << endl;
  //cerr << "EMP: " << empirical << endl; //DE
  vector<weight_t> weights(FD::NumFeats(), 0.0);
  if (conf.count("weights"))
    Weights::InitFromFile(conf["weights"].as<string>(), &weights);
  vector<weight_t> g(FD::NumFeats(), 0.0);
  cerr << "features initialized\noptimizing...\n";
  boost::shared_ptr<BatchOptimizer> o;
  int every = corpus.size() / 20;
  if (!every) ++every;
  o.reset(new LBFGSOptimizer(g.size(), conf["correction_buffers"].as<int>()));
  int iterations = 1000;
  for (int iter = 0; iter < iterations; ++iter) {
    cerr << "ITERATION " << iter << " " << flush;
    fill(g.begin(), g.end(), 0.0);
    for (SparseVector<double>::const_iterator it = empirical.begin(); it != empirical.end(); ++it)
      g[it->first] = -it->second;
    double obj = -empirical.dot(weights);
    // SparseVector<double> mfm;  //DE
    for (int i = 0; i < corpus.size(); ++i) {
      if ((i + 1) % every == 0) cerr << '.' << flush;
      const int num_words = corpus[i].ts.words.size();
      forests[i].Reweight(weights);
      prob_t z;
      forests[i].EdgeMarginals(&z);
      obj -= log(z);
      //cerr << " O = " << (-corpus[i].features.dot(weights)) << " D=" << -lz << "  OO= " << (-corpus[i].features.dot(weights) - lz) << endl;
      //cerr << " ZZ = " << zz << endl;
      for (int h = -1; h < num_words; ++h) {
        for (int m = 0; m < num_words; ++m) {
          if (h == m) continue;
          const ArcFactoredForest::Edge& edge = forests[i](h,m);
          const SparseVector<weight_t>& fmap = edge.features;
          double prob = edge.edge_prob.as_float();
          if (prob < -0.000001) { cerr << "Prob < 0: " << prob << endl; prob = 0; }
          if (prob > 1.000001) { cerr << "Prob > 1: " << prob << endl; prob = 1; }
          AddFeatures(prob, fmap, &g);
          //mfm += fmap * prob;  // DE
        }
      }
    }
    //cerr << endl << "E: " << empirical << endl;  // DE
    //cerr << "M: " << mfm << endl;  // DE
    double r = ApplyRegularizationTerms(conf["regularization_strength"].as<double>(), weights, &g);
    double gnorm = 0;
    for (int i = 0; i < g.size(); ++i)
      gnorm += g[i]*g[i];
    ostringstream ll;
    ll << "ITER=" << (iter+1) << "\tOBJ=" << (obj+r) << "\t[F=" << obj << " R=" << r << "]\tGnorm=" << sqrt(gnorm);
    cerr << ' ' << ll.str().substr(ll.str().find('\t')+1) << endl;
    obj += r;
    assert(obj >= 0);
    o->Optimize(obj, g, &weights);
    Weights::ShowLargestFeatures(weights);
    const bool converged = o->HasConverged();
    const char* ofname = converged ? "weights.final.gz" : "weights.cur.gz";
    if (converged || ((iter+1) % conf["output_every_i_iterations"].as<unsigned>()) == 0) {
      cerr << "writing..." << flush;
      const string sl = ll.str();
      Weights::WriteToFile(ofname, weights, true, &sl);
      cerr << "done" << endl;
    }
    if (converged) { cerr << "CONVERGED\n"; break; }
  }
  return 0;
}

