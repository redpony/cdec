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

using namespace std;
namespace po = boost::program_options;

typedef SparseVector<double> Dir;

MT19937 rng;

struct oracle_directions {
  string forest_repository;
  unsigned dev_set_size;
  vector<Dir> dirs; //best_to_hope_dirs
  vector<int> fids;
  string forest_file(unsigned i) const {
    ostringstream o;
    o << forest_repository << '/' << i << ".json.gz";
    return o.str();
  }

  void set_dev_set_size(int i) {
    dev_set_size=i;
    dirs.resize(dev_set_size);
  }

  oracle_directions(string forest_repository="",unsigned dev_set_sz=0,vector<int> const& fids=vector<int>()): forest_repository(forest_repository),fids(fids) {
    set_dev_set_size(dev_set_sz);
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
  void add_directions(vector<Dir> &dirs,unsigned n,unsigned batchsz=20,bool start_random=false) {
    MT19937::IntRNG rsg=rng.inclusive(0,dev_set_size-1);
    unsigned b=0;
    for(unsigned i=0;i<n;++i) {
      dirs.push_back(Dir());
      Dir &d=dirs.back();
      for (unsigned j=0;j<batchsz;++j,++b)
        d+=(*this)[(start_random || b>=dev_set_size)?rsg():b];
      d/=(double)batchsz;
    }
  }

};

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



void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  OracleBleu::AddOptions(&opts);
  opts.add_options()
        ("dev_set_size,s",po::value<unsigned int>(),"[REQD] Development set size (# of parallel sentences)")
        ("forest_repository,r",po::value<string>(),"[REQD] Path to forest repository")
        ("weights,w",po::value<string>(),"[REQD] Current feature weights file")
        ("optimize_feature,o",po::value<vector<string> >(), "Feature to optimize (if none specified, all weights listed in the weights file will be optimized)")
        ("random_directions,d",po::value<unsigned int>()->default_value(20),"Number of random directions to run the line optimizer in")
    ("no_primary,n","don't use the primary (orthogonal each feature alone) directions")
    ("oracle_directions,O",po::value<unsigned>()->default_value(0),"read the forests and choose this many directions based on heading toward a hope max (bleu+modelscore) translation.")
    ("oracle_batch,b",po::value<unsigned>()->default_value(10),"to produce each oracle direction, sum the 'gradient' over this many sentences")
    ("max_similarity,m",po::value<double>()->default_value(0),"remove directions that are too similar (Tanimoto coeff. less than (1-this)).  0 means don't filter, 1 means only 1 direction allowed?")
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

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  Weights weights;
  vector<string> features;
  weights.InitFromFile(conf["weights"].as<string>(), &features);
  vector<int> fids(features.size());
  for (int i = 0; i < features.size(); ++i)
    fids[i] = FD::Convert(features[i]);

  oracle_directions od(conf["forest_repository"].as<string>()
                       , conf["dev_set_size"].as<unsigned int>()
                       , fids
    );
;
  assert(DirectoryExists(od.forest_repository));
  SparseVector<double> origin;
  weights.InitSparseVector(&origin);
  if (conf.count("optimize_feature") > 0)
    features=conf["optimize_feature"].as<vector<string> >();
  vector<SparseVector<double> > axes;
  LineOptimizer::CreateOptimizationDirections(
     fids,
     conf["random_directions"].as<unsigned int>(),
     &rng,
     &axes,
     !conf.count("no_primary")
    );
  od.add_directions(axes,conf["oracle_directions"].as<unsigned>(),conf["oracle_batch"].as<unsigned>());
  compress_similar(axes,conf["max_similarity"].as<double>());
  for (int i = 0; i < od.dev_set_size; ++i)
    for (int j = 0; j < axes.size(); ++j)
      cout << od.forest_file(i) <<" " << i << ' ' << origin << ' ' << axes[j] << endl;
  return 0;
}
