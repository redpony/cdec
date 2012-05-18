#include <cstdlib>
#include <iostream>
#include <vector>
#include <tr1/unordered_map>
#include <limits>
#include <cmath>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "json_feature_map_lexer.h"
#include "prob.h"
#include "filelib.h"
#include "weights.h"
#include "sparse_vector.h"
#include "liblbfgs/lbfgs++.h"

using namespace std;
using namespace std::tr1;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("training_features,x", po::value<string>(), "File containing training instance features (ARKRegression format)")
        ("training_responses,y", po::value<string>(), "File containing training response features (ARKRegression format)")
        ("linear,n", "Linear (rather than logistic) regression")
        ("l1",po::value<double>()->default_value(0.0), "l_1 regularization strength")
        ("l2",po::value<double>()->default_value(0.0), "l_2 regularization strength")
        ("test_features,t", po::value<string>(), "File containing training instance features (ARKRegression format)")
        ("test_responses,s", po::value<string>(), "File containing training response features (ARKRegression format)")
        ("weights,w", po::value<string>(), "Initial weights")
        ("epsilon,e", po::value<double>()->default_value(1e-4), "Epsilon for convergence test. Terminates when ||g|| < epsilon * max(1, ||w||)")
        ("memory_buffers,m",po::value<unsigned>()->default_value(40), "Number of memory buffers for LBFGS")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("help") || !conf->count("training_features") || !conf->count("training_responses")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

struct TrainingInstance {
  SparseVector<float> x;
  union {
    unsigned label;  // for categorical predictions
    float value;     // for continuous predictions
  } y;
};

struct ReaderHelper {
  explicit ReaderHelper(vector<TrainingInstance>* xyp) : xy_pairs(xyp), lc(), flag() {}
  unordered_map<string, unsigned> id2ind;
  vector<TrainingInstance>* xy_pairs;
  int lc;
  bool flag;
};

void ReaderCB(const string& id, const SparseVector<float>& fmap, void* extra) {
  ReaderHelper& rh = *reinterpret_cast<ReaderHelper*>(extra);
  ++rh.lc;
  if (rh.lc % 1000 == 0) { cerr << '.'; rh.flag = true; }
  if (rh.lc % 40000 == 0) { cerr << " [" << rh.lc << "]\n"; rh.flag = false; }
  const unordered_map<string, unsigned>::iterator it = rh.id2ind.find(id);
  if (it == rh.id2ind.end()) {
    cerr << "Unlabeled example in line " << rh.lc << " (key=" << id << ')' << endl;
    abort();
  }
  (*rh.xy_pairs)[it->second - 1].x = fmap;
}

void ReadLabeledInstances(const string& ffeats,
                 const string& fresp,
                 const bool is_continuous,
                 vector<TrainingInstance>* xy_pairs,
                 vector<string>* labels) {
  bool flag = false;
  xy_pairs->clear();
  int lc = 0;
  ReaderHelper rh(xy_pairs);
  unordered_map<string, unsigned> label2id;
  cerr << "Reading responses from " << fresp << " ..." << endl;
  ReadFile fr(fresp);
  for (unsigned i = 0; i < labels->size(); ++i)
    label2id[(*labels)[i]] = i;
  istream& in = *fr.stream();
  string line;
  while(getline(in, line)) {
    ++lc;
    if (lc % 1000 == 0) { cerr << '.'; flag = true; }
    if (lc % 40000 == 0) { cerr << " [" << lc << "]\n"; flag = false; }
    if (line.size() == 0) continue;
    if (line[0] == '#') continue;
    unsigned p = 0;
    while (p < line.size() && line[p] != ' ' && line[p] != '\t') { ++p; }
    unsigned& ind = rh.id2ind[line.substr(0, p)];
    if (ind != 0) { cerr << "ID " << line.substr(0, p) << " duplicated in line " << lc << endl; abort(); }
    while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) { ++p; }
    assert(p < line.size());
    xy_pairs->push_back(TrainingInstance());
    ind = xy_pairs->size();
    if (is_continuous) {
      xy_pairs->back().y.value = strtof(&line[p], 0);
    } else { // categorical predictions
      unordered_map<string, unsigned>::iterator it = label2id.find(line.substr(p));
      if (it == label2id.end()) {
        const string label = line.substr(p);
        it = label2id.insert(make_pair(label, labels->size())).first;
        labels->push_back(label);
      }
      xy_pairs->back().y.label = it->second;  // label id
    }
  }
  if (flag) cerr << endl;
  if (!is_continuous) {
    cerr << "LABELS:";
    for (unsigned j = 0; j < labels->size(); ++j)
      cerr << " " << (*labels)[j];
    cerr << endl;
  }
  cerr << "Reading features from " << ffeats << " ..." << endl;
  ReadFile ff(ffeats);
  JSONFeatureMapLexer::ReadRules(ff.stream(), ReaderCB, &rh);
  if (rh.flag) cerr << endl;
}

// helper base class (not polymorphic- just a container and some helper functions) for loss functions
// real loss functions should implement double operator()(const vector<double>& x, double* g),
// which should evaluate f(x) and g = f'(x)
struct BaseLoss {
  // dimp1 = number of categorial outputs possible for logistic regression
  // for linear regression, it should be 1 more than the dimension of the response variable
  BaseLoss(
      const vector<TrainingInstance>& tr,
      unsigned dimp1,
      unsigned numfeats,
      unsigned ll2) : training(tr), K(dimp1), p(numfeats), l2(ll2) {}

  // weight vector layout for K classes, with p features
  //   w[0 : K-1] = bias weights
  //   w[y*p + K : y*p + K + p - 1] = feature weights for y^th class
  // this representation is used in ComputeDotProducts and GradAdd
  void ComputeDotProducts(const SparseVector<float>& fx,  // feature vector of x
                          const vector<double>& w,         // full weight vector
                          vector<double>* pdotprods) const {
    vector<double>& dotprods = *pdotprods;
    const unsigned km1 = K - 1;
    dotprods.resize(km1);
    for (unsigned y = 0; y < km1; ++y)
      dotprods[y] = w[y];  // bias terms
    for (SparseVector<float>::const_iterator it = fx.begin(); it != fx.end(); ++it) {
      const float fval = it->second;
      const unsigned fid = it->first;
      for (unsigned y = 0; y < km1; ++y)
        dotprods[y] += w[fid + y * p + km1] * fval;
    }
  }

  double ApplyRegularizationTerms(const vector<double>& weights,
                                  double* g) const {
    double reg = 0;
    for (size_t i = K - 1; i < weights.size(); ++i) {
      const double& w_i = weights[i];
      reg += l2 * w_i * w_i;
      g[i] += 2 * l2 * w_i;
    }
    return reg;
  }

  void GradAdd(const SparseVector<float>& fx,
               const unsigned y,
               const double scale,
               double* acc) const {
    acc[y] += scale; // class bias
    for (SparseVector<float>::const_iterator it = fx.begin();
         it != fx.end(); ++it)
      acc[it->first + y * p + K - 1] += it->second * scale;
  }

  const vector<TrainingInstance>& training;
  const unsigned K, p;
  const double l2;
};

struct UnivariateSquaredLoss : public BaseLoss {
  UnivariateSquaredLoss(
          const vector<TrainingInstance>& tr,
          unsigned numfeats,
          const double l2) : BaseLoss(tr, 2, numfeats, l2) {}

  // evaluate squared loss and gradient
  double operator()(const vector<double>& x, double* g) const {
    fill(g, g + x.size(), 0.0);
    double cll = 0;
    vector<double> dotprods(1);  // univariate prediction
    for (unsigned i = 0; i < training.size(); ++i) {
      const SparseVector<float>& fmapx = training[i].x;
      const double refy = training[i].y.value;
      ComputeDotProducts(fmapx, x, &dotprods);
      double diff = dotprods[0] - refy;
      cll += diff * diff;

      double scale = 2 * diff;
      GradAdd(fmapx, 0, scale, g);
    }
    double reg = ApplyRegularizationTerms(x, g);
    return cll + reg;
  }

  // return root mse
  double Evaluate(const vector<TrainingInstance>& test,
                  const vector<double>& w) const {
    vector<double> dotprods(1);  // K-1 degrees of freedom
    double mse = 0;
    for (unsigned i = 0; i < test.size(); ++i) {
      const SparseVector<float>& fmapx = test[i].x;
      const float refy = test[i].y.value;
      ComputeDotProducts(fmapx, w, &dotprods);
      double diff = dotprods[0] - refy;
      cerr << "line=" << (i+1) << " true=" << refy << " pred=" << dotprods[0] << endl;
      mse += diff * diff;
    }
    mse /= test.size();
    return sqrt(mse);
  }
};

struct MulticlassLogLoss : public BaseLoss {
  MulticlassLogLoss(
          const vector<TrainingInstance>& tr,
          unsigned k,
          unsigned numfeats,
          const double l2) : BaseLoss(tr, k, numfeats, l2) {}

  // evaluate log loss and gradient
  double operator()(const vector<double>& x, double* g) const {
    fill(g, g + x.size(), 0.0);
    vector<double> dotprods(K - 1);  // K-1 degrees of freedom
    vector<prob_t> probs(K);
    double cll = 0;
    for (unsigned i = 0; i < training.size(); ++i) {
      const SparseVector<float>& fmapx = training[i].x;
      const unsigned refy = training[i].y.label;
      //cerr << "FMAP: " << fmapx << endl;
      ComputeDotProducts(fmapx, x, &dotprods);
      prob_t z;
      for (unsigned j = 0; j < dotprods.size(); ++j)
        z += (probs[j] = prob_t(dotprods[j], init_lnx()));
      z += (probs.back() = prob_t::One());
      for (unsigned y = 0; y < probs.size(); ++y) {
        probs[y] /= z;
        //cerr << "  p(y=" << y << ")=" << probs[y].as_float() << "\tz=" << z << endl;
      }
      cll -= log(probs[refy]);  // log p(y | x)

      for (unsigned y = 0; y < dotprods.size(); ++y) {
        double scale = probs[y].as_float();
        if (y == refy) { scale -= 1.0; }
        GradAdd(fmapx, y, scale, g);
      }
    }
    double reg = ApplyRegularizationTerms(x, g);
    return cll + reg;
  }

  double Evaluate(const vector<TrainingInstance>& test,
                  const vector<double>& w) const {
    vector<double> dotprods(K - 1);  // K-1 degrees of freedom
    double correct = 0;
    for (unsigned i = 0; i < test.size(); ++i) {
      const SparseVector<float>& fmapx = test[i].x;
      const unsigned refy = test[i].y.label;
      ComputeDotProducts(fmapx, w, &dotprods);
      double best = 0;
      unsigned besty = dotprods.size();
      for (unsigned y = 0; y < dotprods.size(); ++y)
        if (dotprods[y] > best) { best = dotprods[y]; besty = y; }
      if (refy == besty) { ++correct; }
    }
    return correct / test.size();
  }
};

template <class LossFunction>
double LearnParameters(LossFunction& loss,
                       const double l1,
                       const unsigned l1_start,
                       const unsigned memory_buffers,
                       const double eps,
                       vector<double>* px) {
  LBFGS<LossFunction> lbfgs(px, loss, memory_buffers, l1, l1_start, eps);
  lbfgs.MinimizeFunction();
  return 0;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  string line;
  double l1 = conf["l1"].as<double>();
  double l2 = conf["l2"].as<double>();
  const unsigned memory_buffers = conf["memory_buffers"].as<unsigned>();
  const double epsilon = conf["epsilon"].as<double>();
  if (l1 < 0.0) {
    cerr << "L1 strength must be >= 0\n";
    return 1;
  }
  if (l2 < 0.0) {
    cerr << "L2 strength must be >= 0\n";
    return 2;
  }

  const bool is_continuous = conf.count("linear");
  const string xfile = conf["training_features"].as<string>();
  const string yfile = conf["training_responses"].as<string>();
  vector<string> labels; // only populated for non-continuous models
  vector<TrainingInstance> training, test;
  ReadLabeledInstances(xfile, yfile, is_continuous, &training, &labels);
  if (conf.count("test_features")) {
    const string txfile = conf["test_features"].as<string>();
    const string tyfile = conf["test_responses"].as<string>();
    ReadLabeledInstances(txfile, tyfile, is_continuous, &test, &labels);
  }

  if (conf.count("weights")) {
    cerr << "Initial weights are not implemented, please implement." << endl;
    // TODO read weights for categorical and continuous predictions
    // can't use normal cdec weight framework
    abort();
  }

  cerr << "         Number of features: " << FD::NumFeats() << endl;
  cerr << "Number of training examples: " << training.size() << endl;
  const unsigned p = FD::NumFeats();
  cout.precision(15);

  if (conf.count("linear")) {  // linear regression
    vector<double> weights(1 + FD::NumFeats(), 0.0);
    cerr << "       Number of parameters: " << weights.size() << endl;
    UnivariateSquaredLoss loss(training, p, l2);
    LearnParameters(loss, l1, 1, memory_buffers, epsilon, &weights);

    if (test.size())
      cerr << "Held-out root MSE: " << loss.Evaluate(test, weights) << endl;

    cout << p << "\t***CONTINUOUS***" << endl;
    cout << "***BIAS***\t" << weights[0] << endl;
    for (unsigned f = 0; f < p; ++f) {
      const double w = weights[1 + f];
      if (w)
        cout << FD::Convert(f) << "\t" << w << endl;
    }
  } else {                     // logistic regression
    vector<double> weights((1 + FD::NumFeats()) * (labels.size() - 1), 0.0);
    cerr << "       Number of parameters: " << weights.size() << endl;
    cerr << "           Number of labels: " << labels.size() << endl;
    const unsigned K = labels.size();
    const unsigned km1 = K - 1;
    MulticlassLogLoss loss(training, K, p, l2);
    LearnParameters(loss, l1, km1, memory_buffers, epsilon, &weights);

    if (test.size())
      cerr << "Held-out accuracy: " << loss.Evaluate(test, weights) << endl;

    cout << p << "\t***CATEGORICAL***";
    for (unsigned y = 0; y < K; ++y)
      cout << '\t' << labels[y];
    cout << endl;
    for (unsigned y = 0; y < km1; ++y)
      cout << labels[y] << "\t***BIAS***\t" << weights[y] << endl;
    for (unsigned y = 0; y < km1; ++y) {
      for (unsigned f = 0; f < p; ++f) {
        const double w = weights[km1 + y * p + f];
        if (w)
          cout << labels[y] << "\t" << FD::Convert(f) << "\t" << w << endl;
      }
    }
  }

  return 0;
}

