#include <sstream>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "verbose.h"
#include "hg.h"
#include "prob.h"
#include "inside_outside.h"
#include "ff_register.h"
#include "decoder.h"
#include "filelib.h"
#include "optimize.h"
#include "fdict.h"
#include "weights.h"
#include "sparse_vector.h"

using namespace std;
using boost::shared_ptr;
namespace po = boost::program_options;

void SanityCheck(const vector<double>& w) {
  for (int i = 0; i < w.size(); ++i) {
    assert(!isnan(w[i]));
    assert(!isinf(w[i]));
  }
}

struct FComp {
  const vector<double>& w_;
  FComp(const vector<double>& w) : w_(w) {}
  bool operator()(int a, int b) const {
    return fabs(w_[a]) > fabs(w_[b]);
  }
};

void ShowLargestFeatures(const vector<double>& w) {
  vector<int> fnums(w.size());
  for (int i = 0; i < w.size(); ++i)
    fnums[i] = i;
  vector<int>::iterator mid = fnums.begin();
  mid += (w.size() > 10 ? 10 : w.size());
  partial_sort(fnums.begin(), mid, fnums.end(), FComp(w));
  cerr << "TOP FEATURES:";
  for (vector<int>::iterator i = fnums.begin(); i != mid; ++i) {
    cerr << ' ' << FD::Convert(*i) << '=' << w[*i];
  }
  cerr << endl;
}

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input_weights,w",po::value<string>(),"Input feature weights file")
        ("training_data,t",po::value<string>(),"Training data")
        ("decoder_config,c",po::value<string>(),"Decoder configuration file")
        ("output_weights,o",po::value<string>()->default_value("-"),"Output feature weights file");
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

  if (conf->count("help") || !(conf->count("training_data")) || !conf->count("decoder_config")) {
    cerr << dcmdline_options << endl;
#ifdef HAVE_MPI
    MPI::Finalize();
#endif
    exit(1);
  }
}

void ReadTrainingCorpus(const string& fname, int rank, int size, vector<string>* c) {
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  int lc = 0;
  while(in) {
    getline(in, line);
    if (!in) break;
    if (lc % size == rank) c->push_back(line);
    ++lc;
  }
}

static const double kMINUS_EPSILON = -1e-6;

struct TrainingObserver : public DecoderObserver {
  void Reset() {
    total_complete = 0;
    cur_obj = 0;
    tot_obj = 0;
    tot.clear();
  } 

  void SetLocalGradientAndObjective(SparseVector<double>* g, double* o) const {
    *o = tot_obj;
    *g = tot;
  }

  virtual void NotifyDecodingStart(const SentenceMetadata& smeta) {
    cur_obj = 0;
    state = 1;
  }

  void ExtractExpectedCounts(Hypergraph* hg) {
    vector<prob_t> posts;
    cur.clear();
    const prob_t z = hg->ComputeEdgePosteriors(1.0, &posts);
    cur_obj = log(z);
    for (int i = 0; i < posts.size(); ++i) {
      const SparseVector<double>& efeats = hg->edges_[i].feature_values_;
      const double post = static_cast<double>(posts[i] / z);
      for (SparseVector<double>::const_iterator j = efeats.begin(); j != efeats.end(); ++j)
        cur.add_value(j->first, post);
    }
  }

  // compute model expectations, denominator of objective
  virtual void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    assert(state == 1);
    state = 2;
    ExtractExpectedCounts(hg);
  }

  // replace translation forest, since we're doing EM training (we don't know which)
  virtual void NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    assert(state == 2);
    state = 3;
    ExtractExpectedCounts(hg);
  }

  virtual void NotifyDecodingComplete(const SentenceMetadata& smeta) {
    ++total_complete;
    tot_obj += cur_obj;
    tot += cur;
  }

  int total_complete;
  double cur_obj;
  double tot_obj;
  SparseVector<double> cur, tot;
  int state;
};

void ReadConfig(const string& ini, vector<string>* out) {
  ReadFile rf(ini);
  istream& in = *rf.stream();
  while(in) {
    string line;
    getline(in, line);
    if (!in) continue;
    out->push_back(line);
  }
}

void StoreConfig(const vector<string>& cfg, istringstream* o) {
  ostringstream os;
  for (int i = 0; i < cfg.size(); ++i) { os << cfg[i] << endl; }
  o->str(os.str());
}

struct OptimizableMultinomialFamily {
  struct CPD {
    CPD() : z() {}
    double z;
    map<WordID, double> c2counts;
  };
  map<WordID, CPD> counts;
  double Value(WordID conditioning, WordID generated) const {
    map<WordID, CPD>::const_iterator it = counts.find(conditioning);
    assert(it != counts.end());
    map<WordID,double>::const_iterator r = it->second.c2counts.find(generated);
    if (r == it->second.c2counts.end()) return 0;
    return r->second;
  }
  void Increment(WordID conditioning, WordID generated, double count) {
    CPD& cc = counts[conditioning];
    cc.z += count;
    cc.c2counts[generated] += count;
  }
  void Optimize() {
    for (map<WordID, CPD>::iterator i = counts.begin(); i != counts.end(); ++i) {
      CPD& cpd = i->second;
      for (map<WordID, double>::iterator j = cpd.c2counts.begin(); j != cpd.c2counts.end(); ++j) {
        j->second /= cpd.z;
        // cerr << "P(" << TD::Convert(j->first) << " | " << TD::Convert(i->first) << " ) =  " << j->second << endl;
      }
    }
  }
  void Clear() {
    counts.clear();
  }
};

struct CountManager {
  CountManager(size_t num_types) : oms_(num_types) {}
  virtual ~CountManager();
  virtual void AddCounts(const SparseVector<double>& c) = 0;
  void Optimize(SparseVector<double>* weights) {
    for (int i = 0; i < oms_.size(); ++i) {
      oms_[i].Optimize();
    }
    GetOptimalValues(weights);
    for (int i = 0; i < oms_.size(); ++i) {
      oms_[i].Clear();
    }
  }
  virtual void GetOptimalValues(SparseVector<double>* wv) const = 0;
  vector<OptimizableMultinomialFamily> oms_;
};
CountManager::~CountManager() {}

struct TaggerCountManager : public CountManager {
  // 0 = transitions, 2 = emissions
  TaggerCountManager() : CountManager(2) {}
  void AddCounts(const SparseVector<double>& c);
  void GetOptimalValues(SparseVector<double>* wv) const {
    for (set<int>::const_iterator it = fids_.begin(); it != fids_.end(); ++it) {
      int ftype;
      WordID cond, gen;
      bool is_optimized = TaggerCountManager::GetFeature(*it, &ftype, &cond, &gen);
      assert(is_optimized);
      wv->set_value(*it, log(oms_[ftype].Value(cond, gen)));
    }
  }
  // Id:0:a=1 Bi:a_b=1 Bi:b_c=1 Bi:c_d=1 Uni:a=1 Uni:b=1 Uni:c=1 Uni:d=1 Id:1:b=1 Bi:BOS_a=1 Id:2:c=1
  static bool GetFeature(const int fid, int* feature_type, WordID* cond, WordID* gen) {
    const string& feat = FD::Convert(fid);
    if (feat.size() > 5 && feat[0] == 'I' && feat[1] == 'd' && feat[2] == ':') {
      // emission
      const size_t p = feat.rfind(':');
      assert(p != string::npos);
      *cond = TD::Convert(feat.substr(p+1));
      *gen = TD::Convert(feat.substr(3, p - 3));
      *feature_type = 1;
      return true;
    } else if (feat[0] == 'B' && feat.size() > 5 && feat[2] == ':' && feat[1] == 'i') {
      // transition
      const size_t p = feat.rfind('_');
      assert(p != string::npos);
      *gen = TD::Convert(feat.substr(p+1));
      *cond = TD::Convert(feat.substr(3, p - 3));
      *feature_type = 0;
      return true;
    } else if (feat[0] == 'U' && feat.size() > 4 && feat[1] == 'n' && feat[2] == 'i' && feat[3] == ':') {
      // ignore
      return false;
    } else {
      cerr << "Don't know how to deal with feature of type: " << feat << endl;
      abort();
    }
  }
  set<int> fids_;
};

void TaggerCountManager::AddCounts(const SparseVector<double>& c) {
  for (SparseVector<double>::const_iterator it = c.begin(); it != c.end(); ++it) {
    const double& val = it->second;
    int ftype;
    WordID cond, gen;
    if (GetFeature(it->first, &ftype, &cond, &gen)) {
      oms_[ftype].Increment(cond, gen, val);
      fids_.insert(it->first);
    }
  }
}

int main(int argc, char** argv) {
#ifdef HAVE_MPI
  MPI::Init(argc, argv);
  const int size = MPI::COMM_WORLD.Get_size(); 
  const int rank = MPI::COMM_WORLD.Get_rank();
#else
  const int size = 1;
  const int rank = 0;
#endif
  SetSilent(true);  // turn off verbose decoder output
  register_feature_functions();

  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  TaggerCountManager tcm;

  // load cdec.ini and set up decoder
  vector<string> cdec_ini;
  ReadConfig(conf["decoder_config"].as<string>(), &cdec_ini);
  istringstream ini;
  StoreConfig(cdec_ini, &ini);
  if (rank == 0) cerr << "Loading grammar...\n";
  Decoder* decoder = new Decoder(&ini);
  if (decoder->GetConf()["input"].as<string>() != "-") {
    cerr << "cdec.ini must not set an input file\n";
#ifdef HAVE_MPI
    MPI::COMM_WORLD.Abort(1);
#endif
  }
  if (rank == 0) cerr << "Done loading grammar!\n";
  Weights w;
  if (conf.count("input_weights"))
    w.InitFromFile(conf["input_weights"].as<string>());

  double objective = 0;
  bool converged = false;

  vector<double> lambdas;
  w.InitVector(&lambdas);
  vector<string> corpus;
  ReadTrainingCorpus(conf["training_data"].as<string>(), rank, size, &corpus);
  assert(corpus.size() > 0);

  int iteration = 0;
  TrainingObserver observer;
  while (!converged) {
    ++iteration;
    observer.Reset();
    if (rank == 0) {
      cerr << "Starting decoding... (~" << corpus.size() << " sentences / proc)\n";
    }
    decoder->SetWeights(lambdas);
    for (int i = 0; i < corpus.size(); ++i)
      decoder->Decode(corpus[i], &observer);

    SparseVector<double> x;
    observer.SetLocalGradientAndObjective(&x, &objective);
    cerr << "COUNTS = " << x << endl;
    cerr << "   OBJ = " << objective << endl;
    tcm.AddCounts(x);

#if 0
#ifdef HAVE_MPI
    MPI::COMM_WORLD.Reduce(const_cast<double*>(&gradient.data()[0]), &rcv_grad[0], num_feats, MPI::DOUBLE, MPI::SUM, 0);
    MPI::COMM_WORLD.Reduce(&objective, &to, 1, MPI::DOUBLE, MPI::SUM, 0);
    swap(gradient, rcv_grad);
    objective = to;
#endif
#endif

    if (rank == 0) {
      SparseVector<double> wsv;
      tcm.Optimize(&wsv);

      w.InitFromVector(wsv);
      w.InitVector(&lambdas);

      ShowLargestFeatures(lambdas);

      converged = iteration > 100;
      if (converged) { cerr << "OPTIMIZER REPORTS CONVERGENCE!\n"; }

      string fname = "weights.cur.gz";
      if (converged) { fname = "weights.final.gz"; }
      ostringstream vv;
      vv << "Objective = " << objective << "  (ITERATION=" << iteration << ")";
      const string svv = vv.str();
      w.WriteToFile(fname, true, &svv);
    }  // rank == 0
    int cint = converged;
#ifdef HAVE_MPI
    MPI::COMM_WORLD.Bcast(const_cast<double*>(&lambdas.data()[0]), num_feats, MPI::DOUBLE, 0);
    MPI::COMM_WORLD.Bcast(&cint, 1, MPI::INT, 0);
    MPI::COMM_WORLD.Barrier();
#endif
    converged = cint;
  }
#ifdef HAVE_MPI
  MPI::Finalize(); 
#endif
  return 0;
}
