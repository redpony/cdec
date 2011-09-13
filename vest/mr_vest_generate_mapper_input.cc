//TODO: debug segfault when references supplied, null shared_ptr when oracle
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

const bool DEBUG_ORACLE=true;

//TODO: decide on cdec_ff ffs, or just bleumodel - if just bleumodel, then do existing features on serialized hypergraphs remain?  weights (origin) is passed to oracle_bleu.h:ComputeOracle
//void register_feature_functions();
//FFRegistry ff_registry;
namespace {
void init_bleumodel() {
  ff_registry.clear();
  ff_registry.Register(new FFFactory<BLEUModel>);
}

struct init_ff {
  init_ff() {
    init_bleumodel();
  }
};
//init_ff reg; // order of initialization?  ff_registry may not be init yet.  call in Run() instead.
}

using namespace std;
namespace po = boost::program_options;

typedef SparseVector<double> Dir;
typedef Dir Point;

void compress_similar(vector<Dir> &dirs,double min_dist,ostream *log=&cerr,bool avg=true,bool verbose=true) {
  //  return; //TODO: debug
  if (min_dist<=0) return;
  double max_s=1.-min_dist;
  if (log&&verbose) *log<<"max allowed S="<<max_s<<endl;
  unsigned N=dirs.size();
  for (int i=0;i<N;++i) {
    for (int j=i+1;j<N;++j) {
      double s=dirs[i].tanimoto_coef(dirs[j]);
      if (log&&verbose) *log<<"S["<<i<<","<<j<<"]="<<s<<' ';
      if (s>max_s) {
        if (log) *log << "Collapsing similar directions (T="<<s<<" > "<<max_s<<").  dirs["<<i<<"]="<<dirs[i]<<" dirs["<<j<<"]"<<endl;
        if (avg) {
          dirs[i]+=dirs[j];
          dirs[i]/=2.;
          if (log) *log<<" averaged="<<dirs[i];
        }
        if (log) *log<<endl;
        swap(dirs[j],dirs[--N]);
      }
    }
    if (log&&verbose) *log<<endl;

  }
  dirs.resize(N);
}

struct oracle_directions {
  MT19937 rng;
  OracleBleu oracle;
  vector<Dir> directions;

  bool start_random;
  bool include_primary;
  bool old_to_hope;
  bool fear_to_hope;
  unsigned n_random;
  void AddPrimaryAndRandomDirections() {
    LineOptimizer::CreateOptimizationDirections(
      fids,n_random,&rng,&directions,include_primary);
  }

  void Print() {
    for (int i = 0; i < dev_set_size; ++i)
      for (int j = 0; j < directions.size(); ++j) {
        cout << forest_file(i) <<" " << i<<" ";
        print(cout,origin,"=",";");
        cout<<" ";
        print(cout,directions[j],"=",";");
        cout<<"\n";
      }
  }

  void AddOptions(po::options_description *opts) {
    oracle.AddOptions(opts);
    opts->add_options()
      ("dev_set_size,s",po::value<unsigned>(&dev_set_size),"[REQD] Development set size (# of parallel sentences)")
      ("forest_repository,r",po::value<string>(&forest_repository),"[REQD] Path to forest repository")
      ("weights,w",po::value<string>(&weights_file),"[REQD] Current feature weights file")
      ("optimize_feature,o",po::value<vector<string> >(), "Feature to optimize (if none specified, all weights listed in the weights file will be optimized)")
      ("random_directions,d",po::value<unsigned>(&n_random)->default_value(10),"Number of random directions to run the line optimizer in")
      ("no_primary,n","don't use the primary (orthogonal each feature alone) directions")
      ("oracle_directions,O",po::value<unsigned>(&n_oracle)->default_value(0),"read the forests and choose this many directions based on heading toward a hope max (bleu+modelscore) translation.")
      ("oracle_start_random",po::bool_switch(&start_random),"sample random subsets of dev set for ALL oracle directions, not just those after a sequential run through it")
      ("oracle_batch,b",po::value<unsigned>(&oracle_batch)->default_value(10),"to produce each oracle direction, sum the 'gradient' over this many sentences")
      ("max_similarity,m",po::value<double>(&max_similarity)->default_value(0),"remove directions that are too similar (Tanimoto coeff. less than (1-this)).  0 means don't filter, 1 means only 1 direction allowed?")
      ("fear_to_hope,f",po::bool_switch(&fear_to_hope),"for each of the oracle_directions, also include a direction from fear to hope (as well as origin to hope)")
      ("no_old_to_hope","don't emit the usual old -> hope oracle")
      ("decoder_translations",po::value<string>(&decoder_translations_file)->default_value(""),"one per line decoder 1best translations for computing document BLEU vs. sentences-seen-so-far BLEU")
      ;
  }
  void InitCommandLine(int argc, char *argv[], po::variables_map *conf) {
    po::options_description opts("Configuration options");
    AddOptions(&opts);
    opts.add_options()("help,h", "Help");

    po::options_description dcmdline_options;
    dcmdline_options.add(opts);
    po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
    po::notify(*conf);
    if (conf->count("dev_set_size") == 0) {
      cerr << "Please specify the size of the development set using -s N\n";
      goto bad_cmdline;
    }
    if (conf->count("weights") == 0) {
      cerr << "Please specify the starting-point weights using -w <weightfile.txt>\n";
      goto bad_cmdline;
    }
    if (conf->count("forest_repository") == 0) {
      cerr << "Please specify the forest repository location using -r <DIR>\n";
      goto bad_cmdline;
    }
    if (n_oracle && oracle.refs.empty()) {
      cerr<<"Specify references when using oracle directions\n";
      goto bad_cmdline;
    }
    if (conf->count("help")) {
      cout << dcmdline_options << endl;
      exit(0);
    }

    return;
    bad_cmdline:
      cerr << dcmdline_options << endl;
      exit(1);
  }

  int main(int argc, char *argv[]) {
    po::variables_map conf;
    InitCommandLine(argc,argv,&conf);
    init_bleumodel();
    UseConf(conf);
    Run();
    return 0;
  }
  bool verbose() const { return oracle.verbose; }
  void Run() {
//    register_feature_functions();
    AddPrimaryAndRandomDirections();
    AddOracleDirections();
    compress_similar(directions,max_similarity,&cerr,true,verbose());
    Print();
  }


  Point origin; // old weights that gave model 1best.
  vector<string> optimize_features;
  void UseConf(po::variables_map const& conf) {
    oracle.UseConf(conf);
    include_primary=!conf.count("no_primary");
    old_to_hope=!conf.count("no_old_to_hope");

    if (conf.count("optimize_feature") > 0)
      optimize_features=conf["optimize_feature"].as<vector<string> >();
    Init();
  }

  string weights_file;
  double max_similarity;
  unsigned n_oracle, oracle_batch;
  string forest_repository;
  unsigned dev_set_size;
  vector<Oracle> oracles;
  vector<int> fids;
  string forest_file(unsigned i) const {
    ostringstream o;
    o << forest_repository << '/' << i << ".json.gz";
    return o.str();
  }

  oracle_directions() { }

  Sentences model_hyps;

  vector<ScoreP> model_scores;
  bool have_doc;
  void Init() {
    have_doc=!decoder_translations_file.empty();
    if (have_doc) {
      model_hyps.Load(decoder_translations_file);
      if (verbose()) model_hyps.Print(cerr,5);
      model_scores.resize(model_hyps.size());
      if (dev_set_size!=model_hyps.size()) {
        cerr<<"You supplied decoder_translations with a different number of lines ("<<model_hyps.size()<<") than dev_set_size ("<<dev_set_size<<")"<<endl;
        abort();
      }
      cerr << "Scoring model translations " << model_hyps << endl;
      for (int i=0;i<model_hyps.size();++i) {
        //TODO: what is scoreCcand? without clipping? do without for consistency w/ oracle
        model_scores[i]=oracle.ds[i]->ScoreCandidate(model_hyps[i]);
        assert(model_scores[i]);
        if (verbose()) cerr<<"Before model["<<i<<"]: "<<ds().ScoreDetails()<<endl;
        if (verbose()) cerr<<"model["<<i<<"]: "<<model_scores[i]->ScoreDetails()<<endl;
        oracle.doc_score->PlusEquals(*model_scores[i]);
        if (verbose()) cerr<<"After model["<<i<<"]: "<<ds().ScoreDetails()<<endl;
      }
      //TODO: compute doc bleu stats for each sentence, then when getting oracle temporarily exclude stats for that sentence (skip regular score updating)
    }
    start_random=false;
    cerr << "Forest repo: " << forest_repository << endl;
    assert(DirectoryExists(forest_repository));
    vector<string> features;
    vector<weight_t> dorigin;
    Weights::InitFromFile(weights_file, &dorigin, &features);
    if (optimize_features.size())
      features=optimize_features;
    Weights::InitSparseVector(dorigin, &origin);
    fids.clear();
    AddFeatureIds(features);
    oracles.resize(dev_set_size);
  }

  void AddFeatureIds(vector<string> const& features) {
    int i = fids.size();
    fids.resize(fids.size()+features.size());
    for (; i < features.size(); ++i)
      fids[i] = FD::Convert(features[i]);
 }


  std::string decoder_translations_file; // one per line
  //TODO: is it worthwhile to get a complete document bleu first?  would take a list of 1best translations one per line from the decoders, rather than loading all the forests (expensive).  translations are in run.raw.N.gz - new arg
  void adjust_doc(unsigned i,double scale=1.) {
    oracle.doc_score->PlusEquals(*model_scores[i],scale);
  }

  Score &ds() {
    return *oracle.doc_score;
  }

  Oracle const& ComputeOracle(unsigned i) {
    Oracle &o=oracles[i];
    if (o.is_null()) {
      if (have_doc) {
        if (verbose()) cerr<<"Before removing i="<<i<<" "<<ds().ScoreDetails()<<"\n";
        adjust_doc(i,-1);
      }
      ReadFile rf(forest_file(i));
      Hypergraph hg;
      {
        Timer t("Loading forest from JSON "+forest_file(i));
        HypergraphIO::ReadFromJSON(rf.stream(), &hg);
      }
      if (verbose()) cerr<<"Before oracle["<<i<<"]: "<<ds().ScoreDetails()<<endl;
      o=oracle.ComputeOracle(oracle.MakeMetadata(hg,i),&hg,origin);
      if (verbose()) {
        cerr << o;
        ScoreP hopesc=oracle.GetScore(o.hope.sentence,i);
        oracle.doc_score->PlusEquals(*hopesc,1);
        cerr<<"With hope: "<<ds().ScoreDetails()<<endl;
        oracle.doc_score->PlusEquals(*hopesc,-1);
        cerr<<"Without hope: "<<ds().ScoreDetails()<<endl;
        cerr<<" oracle="<<oracle.GetScore(o.hope.sentence,i)->ScoreDetails()<<endl
            <<" model="<<oracle.GetScore(o.model.sentence,i)->ScoreDetails()<<endl;
        if (have_doc)
          cerr<<" doc (should = model): "<<model_scores[i]->ScoreDetails()<<endl;
      }
      if (have_doc) {
        adjust_doc(i,1);
      } else
        oracle.IncludeLastScore();
    }
    return o;
  }

  // if start_random is true, immediately sample w/ replacement from src sentences; otherwise, consume them sequentially until exhausted, then random.  oracle vectors are summed
  void AddOracleDirections() {
    MT19937::IntRNG rsg=rng.inclusive(0,dev_set_size-1);
    unsigned b=0;
    for(unsigned i=0;i<n_oracle;++i) {
      Dir o2hope;
      Dir fear2hope;
      for (unsigned j=0;j<oracle_batch;++j,++b) {
        Oracle const& o=ComputeOracle((start_random||b>=dev_set_size) ? rsg() : b);

        if (old_to_hope)
          o2hope+=o.ModelHopeGradient();
        if (fear_to_hope)
          fear2hope+=o.FearHopeGradient();
      }
      double N=(double)oracle_batch;
      if (old_to_hope) {
        o2hope/=N;
        directions.push_back(o2hope);
      }
      if (fear_to_hope) {
        fear2hope/=N;
        directions.push_back(fear2hope);
      }
    }
  }
};

int main(int argc, char** argv) {
  oracle_directions od;
  return od.main(argc,argv);
}
