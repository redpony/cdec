#include "arc_factored.h"

#include <vector>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "arc_ff.h"
#include "arc_ff_factory.h"
#include "stringlib.h"
#include "filelib.h"
#include "tdict.h"
#include "picojson.h"
#include "optimize.h"
#include "weights.h"
#include "rst.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  string cfg_file;
  opts.add_options()
        ("training_data,t",po::value<string>()->default_value("-"), "File containing training data (jsent format)")
        ("feature_function,F",po::value<vector<string> >()->composing(), "feature function")
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

struct TrainingInstance {
  TaggedSentence ts;
  EdgeSubset tree;
  SparseVector<weight_t> features;
};

void ReadTraining(const string& fname, vector<TrainingInstance>* corpus, int rank = 0, int size = 1) {
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  string err;
  int lc = 0;
  bool flag = false;
  while(getline(in, line)) {
    ++lc;
    if ((lc-1) % size != rank) continue;
    if (rank == 0 && lc % 10 == 0) { cerr << '.' << flush; flag = true; }
    if (rank == 0 && lc % 400 == 0) { cerr << " [" << lc << "]\n"; flag = false; }
    size_t pos = line.rfind('\t');
    assert(pos != string::npos);
    picojson::value obj;
    picojson::parse(obj, line.begin() + pos, line.end(), &err);
    if (err.size() > 0) { cerr << "JSON parse error in " << lc << ": " << err << endl; abort(); }
    corpus->push_back(TrainingInstance());
    TrainingInstance& cur = corpus->back();
    TaggedSentence& ts = cur.ts;
    EdgeSubset& tree = cur.tree;
    assert(obj.is<picojson::object>());
    const picojson::object& d = obj.get<picojson::object>();
    const picojson::array& ta = d.find("tokens")->second.get<picojson::array>();
    for (unsigned i = 0; i < ta.size(); ++i) {
      ts.words.push_back(TD::Convert(ta[i].get<picojson::array>()[0].get<string>()));
      ts.pos.push_back(TD::Convert(ta[i].get<picojson::array>()[1].get<string>()));
    }
    const picojson::array& da = d.find("deps")->second.get<picojson::array>();
    for (unsigned i = 0; i < da.size(); ++i) {
      const picojson::array& thm = da[i].get<picojson::array>();
      // get dep type here
      short h = thm[2].get<double>();
      short m = thm[1].get<double>();
      if (h < 0)
        tree.roots.push_back(m);
      else
        tree.h_m_pairs.push_back(make_pair(h,m));
    }
    //cerr << TD::GetString(ts.words) << endl << TD::GetString(ts.pos) << endl << tree << endl;
  }
  if (flag) cerr << "\nRead " << lc << " training instances\n";
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
  ArcFactoredForest af(5);
  ArcFFRegistry reg;
  reg.Register("DistancePenalty", new ArcFFFactory<DistancePenalty>);
  vector<TrainingInstance> corpus;
  vector<boost::shared_ptr<ArcFeatureFunction> > ffs;
  ffs.push_back(boost::shared_ptr<ArcFeatureFunction>(new DistancePenalty("")));
  ReadTraining(conf["training_data"].as<string>(), &corpus, rank, size);
  vector<ArcFactoredForest> forests(corpus.size());
  SparseVector<double> empirical;
  bool flag = false;
  for (int i = 0; i < corpus.size(); ++i) {
    TrainingInstance& cur = corpus[i];
    if (rank == 0 && (i+1) % 10 == 0) { cerr << '.' << flush; flag = true; }
    if (rank == 0 && (i+1) % 400 == 0) { cerr << " [" << (i+1) << "]\n"; flag = false; }
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
  //cerr << "EMP: " << empirical << endl; //DE
  vector<weight_t> weights(FD::NumFeats(), 0.0);
  vector<weight_t> g(FD::NumFeats(), 0.0);
  cerr << "features initialized\noptimizing...\n";
  boost::shared_ptr<BatchOptimizer> o;
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
      const int num_words = corpus[i].ts.words.size();
      forests[i].Reweight(weights);
      double lz;
      forests[i].EdgeMarginals(&lz);
      obj -= lz;
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
    cerr << endl << ll.str() << endl;
    obj += r;
    assert(obj >= 0);
    o->Optimize(obj, g, &weights);
    Weights::ShowLargestFeatures(weights);
    string sl = ll.str();
    Weights::WriteToFile(o->HasConverged() ? "weights.final.gz" : "weights.cur.gz", weights, true, &sl);
    if (o->HasConverged()) { cerr << "CONVERGED\n"; break; }
  }
  forests[0].Reweight(weights);
  TreeSampler ts(forests[0]);
  EdgeSubset tt; ts.SampleRandomSpanningTree(&tt);
  return 0;
}

