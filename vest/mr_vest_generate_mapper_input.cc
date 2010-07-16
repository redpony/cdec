#include <iostream>
#include <vector>
#include <sstream>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "sampler.h"
#include "filelib.h"
#include "weights.h"
#include "line_optimizer.h"
#include "hg.h"
#include "hg_io.h"
#include "scorer.h"
#include "oracle_bleu.h"
#include "ff_bleu.h"

boost::shared_ptr<FFRegistry> global_ff_registry;
namespace {
struct init_ff {
  init_ff() {
    global_ff_registry.reset(new FFRegistry);
    global_ff_registry->Register(new FFFactory<BLEUModel>);
  }
};
init_ff reg;
}

using namespace std;
namespace po = boost::program_options;

typedef SparseVector<double> Dir;
typedef Dir Point;


void compress_similar(vector<Dir> &dirs,double min_dist,ostream *log=&cerr,bool avg=true) {
  if (min_dist<=0) return;
  double max_s=1.-min_dist;
  unsigned N=dirs.size();
  for (int i=0;i<N;++i) {
    for (int j=i+1;j<N;++j) {
      double s=dirs[i].tanimoto_coef(dirs[j]);
      if (s>max_s) {
        if (log) *log << "Collapsing similar directions (T="<<s<<" > "<<max_s<<").  dirs["<<i<<"]="<<dirs[i]<<" dirs["<<j<<"]";
        if (avg) {
          dirs[i]+=dirs[j];
          dirs[i]/=2.;
          if (log) *log<<" averaged="<<dirs[i];
        }
        if (log) *log<<endl;
        swap(dirs[j],dirs[--N]);
      }
    }
  }
  dirs.resize(N);
}

struct oracle_directions {
  MT19937 rng;
  OracleBleu oracle;
  vector<Dir> directions;

  bool start_random;
  bool include_primary;
  bool fear_to_hope;
  unsigned n_random;
  void AddPrimaryAndRandomDirections() {
    LineOptimizer::CreateOptimizationDirections(
      fids,n_random,&rng,&directions,include_primary);
  }

  void Print() {
    for (int i = 0; i < dev_set_size; ++i)
      for (int j = 0; j < directions.size(); ++j)
        cout << forest_file(i) <<" " << i << ' ' << origin << ' ' << directions[j] << endl;
  }

  void AddOptions(po::options_description *opts) {
    oracle.AddOptions(opts);
  }

  void InitCommandLine(int argc, char *argv[], po::variables_map *conf) {
    po::options_description opts("Configuration options");
    OracleBleu::AddOptions(&opts);
    opts.add_options()
      ("dev_set_size,s",po::value<unsigned>(&dev_set_size),"[REQD] Development set size (# of parallel sentences)")
      ("forest_repository,r",po::value<string>(),"[REQD] Path to forest repository")
      ("weights,w",po::value<string>(),"[REQD] Current feature weights file")
      ("optimize_feature,o",po::value<vector<string> >(), "Feature to optimize (if none specified, all weights listed in the weights file will be optimized)")
      ("random_directions,d",po::value<unsigned int>()->default_value(20),"Number of random directions to run the line optimizer in")
      ("no_primary,n","don't use the primary (orthogonal each feature alone) directions")
      ("oracle_directions,O",po::value<unsigned>()->default_value(0),"read the forests and choose this many directions based on heading toward a hope max (bleu+modelscore) translation.")
      ("oracle_start_random",po::bool_switch(&start_random),"sample random subsets of dev set for ALL oracle directions, not just those after a sequential run through it")
      ("oracle_batch,b",po::value<unsigned>()->default_value(10),"to produce each oracle direction, sum the 'gradient' over this many sentences")
      ("max_similarity,m",po::value<double>()->default_value(0),"remove directions that are too similar (Tanimoto coeff. less than (1-this)).  0 means don't filter, 1 means only 1 direction allowed?")
      ("fear_to_hope,f","for each of the oracle_directions, also include a direction from fear to hope (as well as origin to hope)")
      ("help,h", "Help");
    po::options_description dcmdline_options;
    dcmdline_options.add(opts);
    po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
    bool flag = false;
    if (conf->count("dev_set_size") == 0) {
      cerr << "Please specify the size of the development set using -d N\n";
      flag = true;
    }
    if (conf->count("weights") == 0) {
      cerr << "Please specify the starting-point weights using -w <weightfile.txt>\n";
      flag = true;
    }
    if (conf->count("forest_repository") == 0) {
      cerr << "Please specify the forest repository location using -r <DIR>\n";
      flag = true;
    }
    if (flag || conf->count("help")) {
      cerr << dcmdline_options << endl;
      exit(1);
    }
  }

  int main(int argc, char *argv[]) {
    po::variables_map conf;
    InitCommandLine(argc,argv,&conf);
    UseConf(conf);
    Run();
    return 0;
  }

  void Run() {
    AddPrimaryAndRandomDirections();
    AddOracleDirections();
    compress_similar(directions,max_similarity);
    Print();
  }


  Point origin; // old weights that gave model 1best.
  vector<string> optimize_features;
  void UseConf(po::variables_map const& conf) {
    oracle.UseConf(conf);

    include_primary=!conf.count("no_primary");
    if (conf.count("optimize_feature") > 0)
      optimize_features=conf["optimize_feature"].as<vector<string> >();
    fear_to_hope=conf.count("fear_to_hope");
    n_random=conf["random_directions"].as<unsigned int>();
    forest_repository=conf["forest_repository"].as<string>();
//    dev_set_size=conf["dev_set_size"].as<unsigned int>();
    n_oracle=conf["oracle_directions"].as<unsigned>();
    oracle_batch=conf["oracle_batch"].as<unsigned>();
    max_similarity=conf["max_similarity"].as<double>();
    weights_file=conf["weights"].as<string>();

    Init();
  }

  string weights_file;
  double max_similarity;
  unsigned n_oracle, oracle_batch;
  string forest_repository;
  unsigned dev_set_size;
  vector<Dir> dirs; //best_to_hope_dirs
  vector<int> fids;
  string forest_file(unsigned i) const {
    ostringstream o;
    o << forest_repository << '/' << i << ".json.gz";
    return o.str();
  }

  oracle_directions() { }

  void Init() {
    start_random=false;
    assert(DirectoryExists(forest_repository));
    vector<string> features;
    weights.InitFromFile(weights_file, &features);
    if (optimize_features.size())
      features=optimize_features;
    weights.InitSparseVector(&origin);
    fids.clear();
    AddFeatureIds(features);
  }

  Weights weights;
  void AddFeatureIds(vector<string> const& features) {
    int i = fids.size();
    fids.resize(fids.size()+features.size());
    for (; i < features.size(); ++i)
      fids[i] = FD::Convert(features[i]);
 }


  Dir const& operator[](unsigned i) {
    Dir &dir=dirs[i];
    if (dir.empty()) {
      ReadFile rf(forest_file(i));
      FeatureVector fear,hope,best;
      //TODO: get hope/oracle from vlad.  random for now.
      LineOptimizer::RandomUnitVector(fids,&dir,&rng);
    }
    return dir;
  }
  // if start_random is true, immediately sample w/ replacement from src sentences; otherwise, consume them sequentially until exhausted, then random.  oracle vectors are summed
  void AddOracleDirections() {
    MT19937::IntRNG rsg=rng.inclusive(0,dev_set_size-1);
    unsigned b=0;
    for(unsigned i=0;i<n_oracle;++i) {
      directions.push_back(Dir());
      Dir &d=directions.back();
      for (unsigned j=0;j<oracle_batch;++j,++b)
        d+=(*this)[(start_random || b>=dev_set_size)?rsg():b];
      d/=(double)oracle_batch;
    }
  }
};

int main(int argc, char** argv) {
  oracle_directions od;
  return od.main(argc,argv);
}
