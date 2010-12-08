#include "decoder.h"

#include <tr1/unordered_map>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "sampler.h"
#include "stringlib.h"
#include "weights.h"
#include "filelib.h"
#include "fdict.h"
#include "timing_stats.h"
#include "verbose.h"

#include "translator.h"
#include "phrasebased_translator.h"
#include "tagger.h"
#include "lextrans.h"
#include "lexalign.h"
#include "csplit.h"

#include "lattice.h"
#include "hg.h"
#include "sentence_metadata.h"
#include "hg_intersect.h"

#include "apply_fsa_models.h"
#include "oracle_bleu.h"
#include "apply_models.h"
#include "ff.h"
#include "ff_factory.h"
#include "cfg_options.h"
#include "viterbi.h"
#include "kbest.h"
#include "inside_outside.h"
#include "exp_semiring.h"
#include "sentence_metadata.h"
#include "hg_cfg.h"

#include "forest_writer.h" // TODO this section should probably be handled by an Observer
#include "hg_io.h"
#include "aligner.h"
static const double kMINUS_EPSILON = -1e-6;  // don't be too strict

using namespace std;
using namespace std::tr1;
using boost::shared_ptr;
namespace po = boost::program_options;

static bool verbose_feature_functions=true;

namespace Hack { void MaxTrans(const Hypergraph& in, int beam_size); }
namespace NgramCache { void Clear(); }

DecoderObserver::~DecoderObserver() {}
void DecoderObserver::NotifyDecodingStart(const SentenceMetadata& smeta) {}
void DecoderObserver::NotifySourceParseFailure(const SentenceMetadata&) {}
void DecoderObserver::NotifyTranslationForest(const SentenceMetadata&, Hypergraph*) {}
void DecoderObserver::NotifyAlignmentFailure(const SentenceMetadata&) {}
void DecoderObserver::NotifyAlignmentForest(const SentenceMetadata&, Hypergraph*) {}
void DecoderObserver::NotifyDecodingComplete(const SentenceMetadata&) {}

struct ELengthWeightFunction {
  double operator()(const Hypergraph::Edge& e) const {
    return e.rule_->ELength() - e.rule_->Arity();
  }
};
inline void ShowBanner() {
  cerr << "cdec v1.0 (c) 2009-2010 by Chris Dyer\n";
}

inline void show_models(po::variables_map const& conf,ModelSet &ms,char const* header) {
  cerr<<header<<": ";
  ms.show_features(cerr,cerr,conf.count("warn_0_weight"));
}

inline string str(char const* name,po::variables_map const& conf) {
  return conf[name].as<string>();
}

inline bool prelm_weights_string(po::variables_map const& conf,string &s) {
  if (conf.count("prelm_weights")) {
    s=str("prelm_weights",conf);
    return true;
  }
  if (conf.count("prelm_copy_weights")) {
    s=str("weights",conf);
    return true;
  }
  return false;
}



// print just the --long_opt names suitable for bash compgen
inline void print_options(std::ostream &out,po::options_description const& opts) {
  typedef std::vector< shared_ptr<po::option_description> > Ds;
  Ds const& ds=opts.options();
  out << '"';
  for (unsigned i=0;i<ds.size();++i) {
    if (i) out<<' ';
    out<<"--"<<ds[i]->long_name();
  }
  out << '"';
}

template <class V>
inline bool store_conf(po::variables_map const& conf,std::string const& name,V *v) {
  if (conf.count(name)) {
    *v=conf[name].as<V>();
    return true;
  }
  return false;
}

inline shared_ptr<FeatureFunction> make_ff(string const& ffp,bool verbose_feature_functions,char const* pre="") {
  string ff, param;
  SplitCommandAndParam(ffp, &ff, &param);
  cerr << pre << "feature: " << ff;
  if (param.size() > 0) cerr << " (with config parameters '" << param << "')\n";
  else cerr << " (no config parameters)\n";
  shared_ptr<FeatureFunction> pf = ff_registry.Create(ff, param);
  if (!pf) exit(1);
  int nbyte=pf->NumBytesContext();
  if (verbose_feature_functions)
    cerr<<"State is "<<nbyte<<" bytes for "<<pre<<"feature "<<ffp<<endl;
  return pf;
}

inline shared_ptr<FsaFeatureFunction> make_fsa_ff(string const& ffp,bool verbose_feature_functions,char const* pre="") {
  string ff, param;
  SplitCommandAndParam(ffp, &ff, &param);
  cerr << "FSA Feature: " << ff;
  if (param.size() > 0) cerr << " (with config parameters '" << param << "')\n";
  else cerr << " (no config parameters)\n";
  shared_ptr<FsaFeatureFunction> pf = fsa_ff_registry.Create(ff, param);
  if (!pf) exit(1);
  if (verbose_feature_functions)
    cerr<<"State is "<<pf->state_bytes()<<" bytes for "<<pre<<"feature "<<ffp<<endl;
  return pf;
}

struct DecoderImpl {
  DecoderImpl(po::variables_map& conf, int argc, char** argv, istream* cfg);
  ~DecoderImpl();
  bool Decode(const string& input, DecoderObserver*);
  void SetWeights(const vector<double>& weights) {
    feature_weights = weights;
  }
  void SetId(int next_sent_id) { sent_id = next_sent_id - 1; }

  void forest_stats(Hypergraph &forest,string name,bool show_tree,bool show_features,WeightVector *weights=0,bool show_deriv=false) {
    cerr << viterbi_stats(forest,name,true,show_tree,show_deriv);
    if (show_features) {
      cerr << name<<"     features: ";
/*      Hypergraph::Edge const* best=forest.ViterbiGoalEdge();
      if (!best)
        cerr << name<<" has no goal edge.";
      else
        cerr<<best->feature_values_;
*/
    cerr << ViterbiFeatures(forest,weights);
    cerr << endl;
    }
  }

  void forest_stats(Hypergraph &forest,string name,bool show_tree,bool show_features,DenseWeightVector const& feature_weights, bool sd=false) {
      WeightVector fw(feature_weights);
      forest_stats(forest,name,show_tree,show_features,&fw,sd);
  }

  bool beam_param(po::variables_map const& conf,string const& name,double *val,bool scale_srclen=false,double srclen=1) {
    if (conf.count(name)) {
      *val=conf[name].as<double>()*(scale_srclen?srclen:1);
      return true;
    }
    return false;
  }

  void maybe_prune(Hypergraph &forest,po::variables_map const& conf,string nbeam,string ndensity,string forestname,double srclen) {
    double beam_prune=0,density_prune=0;
    bool use_beam_prune=beam_param(conf,nbeam,&beam_prune,conf.count("scale_prune_srclen"),srclen);
    bool use_density_prune=beam_param(conf,ndensity,&density_prune);
    if (use_beam_prune || use_density_prune) {
      double presize=forest.edges_.size();
      vector<bool> preserve_mask,*pm=0;
      if (conf.count("csplit_preserve_full_word")) {
        preserve_mask.resize(forest.edges_.size());
        preserve_mask[CompoundSplit::GetFullWordEdgeIndex(forest)] = true;
        pm=&preserve_mask;
      }
      forest.PruneInsideOutside(beam_prune,density_prune,pm,false,1,conf["promise_power"].as<double>());
      if (!forestname.empty()) forestname=" "+forestname;
      forest_stats(forest,"  Pruned "+forestname+" forest",false,false,0,false);
      cerr << "  Pruned "<<forestname<<" forest portion of edges kept: "<<forest.edges_.size()/presize<<endl;
    }
  }

  void SampleRecurse(const Hypergraph& hg, const vector<SampleSet<prob_t> >& ss, int n, vector<WordID>* out) {
    const SampleSet<prob_t>& s = ss[n];
    int i = rng->SelectSample(s);
    const Hypergraph::Edge& edge = hg.edges_[hg.nodes_[n].in_edges_[i]];
    vector<vector<WordID> > ants(edge.tail_nodes_.size());
    for (int j = 0; j < ants.size(); ++j)
      SampleRecurse(hg, ss, edge.tail_nodes_[j], &ants[j]);

    vector<const vector<WordID>*> pants(ants.size());
    for (int j = 0; j < ants.size(); ++j) pants[j] = &ants[j];
    edge.rule_->ESubstitute(pants, out);
  }

  struct SampleSort {
    bool operator()(const pair<int,string>& a, const pair<int,string>& b) const {
      return a.first > b.first;
    }
  };

  // TODO this should be handled by an Observer
  void MaxTranslationSample(Hypergraph* hg, const int samples, const int k) {
    unordered_map<string, int, boost::hash<string> > m;
    hg->PushWeightsToGoal();
    const int num_nodes = hg->nodes_.size();
    vector<SampleSet<prob_t> > ss(num_nodes);
    for (int i = 0; i < num_nodes; ++i) {
      SampleSet<prob_t>& s = ss[i];
      const vector<int>& in_edges = hg->nodes_[i].in_edges_;
      for (int j = 0; j < in_edges.size(); ++j) {
        s.add(hg->edges_[in_edges[j]].edge_prob_);
      }
    }
    for (int i = 0; i < samples; ++i) {
      vector<WordID> yield;
      SampleRecurse(*hg, ss, hg->nodes_.size() - 1, &yield);
      const string trans = TD::GetString(yield);
      ++m[trans];
    }
    vector<pair<int, string> > dist;
    for (unordered_map<string, int, boost::hash<string> >::iterator i = m.begin();
           i != m.end(); ++i) {
      dist.push_back(make_pair(i->second, i->first));
    }
    sort(dist.begin(), dist.end(), SampleSort());
    if (k) {
      for (int i = 0; i < k; ++i)
        cout << dist[i].first << " ||| " << dist[i].second << endl;
    } else {
      cout << dist[0].second << endl;
    }
  }

  void ParseTranslatorInputLattice(const string& line, string* input, Lattice* ref) {
    string sref;
    ParseTranslatorInput(line, input, &sref);
    if (sref.size() > 0) {
      assert(ref);
      LatticeTools::ConvertTextOrPLF(sref, ref);
    }
  } 

  po::variables_map& conf;
  OracleBleu oracle;
  CFGOptions cfg_options;
  string formalism;
  shared_ptr<Translator> translator;
  vector<double> feature_weights,prelm_feature_weights;
  Weights w,prelm_w;
  vector<shared_ptr<FeatureFunction> > pffs,prelm_only_ffs;
  vector<const FeatureFunction*> late_ffs,prelm_ffs;
  vector<shared_ptr<FsaFeatureFunction> > fsa_ffs;
  vector<string> fsa_names;
  ModelSet* late_models, *prelm_models;
  IntersectionConfiguration* inter_conf;
  shared_ptr<RandomNumberGenerator<boost::mt19937> > rng;
  int sample_max_trans;
  bool aligner_mode;
  bool minimal_forests;
  bool graphviz; 
  bool joshua_viz;
  bool encode_b64;
  bool kbest;
  bool unique_kbest;
  bool crf_uniform_empirical;
  bool get_oracle_forest;
  shared_ptr<WriteFile> extract_file;
  int combine_size;
  int sent_id;
  SparseVector<prob_t> acc_vec;  // accumulate gradient
  double acc_obj; // accumulate objective
  int g_count;    // number of gradient pieces computed
  bool has_prelm_models;
  int pop_limit;
  bool csplit_output_plf;
  bool write_gradient; // TODO Observer
  bool feature_expectations; // TODO Observer
  bool output_training_vector; // TODO Observer

  static void ConvertSV(const SparseVector<prob_t>& src, SparseVector<double>* trg) {
    for (SparseVector<prob_t>::const_iterator it = src.begin(); it != src.end(); ++it)
      trg->set_value(it->first, it->second);
  }
};

DecoderImpl::~DecoderImpl() {
  if (output_training_vector && !acc_vec.empty()) {
    if (encode_b64) {
      cout << "0\t";
      SparseVector<double> dav; ConvertSV(acc_vec, &dav);
      B64::Encode(acc_obj, dav, &cout);
      cout << endl << flush;
    } else {
      cout << "0\t**OBJ**=" << acc_obj << ';' << acc_vec << endl << flush;
    }
  }
}

DecoderImpl::DecoderImpl(po::variables_map& conf, int argc, char** argv, istream* cfg) : conf(conf) {
  if (cfg) { if (argc || argv) { cerr << "DecoderImpl() can only take a file or command line options, not both\n"; exit(1); } }
  bool show_config;
  bool show_weights;
  vector<string> cfg_files;

  po::options_description opts("Configuration options");
  opts.add_options()
        ("formalism,f",po::value<string>(),"Decoding formalism; values include SCFG, FST, PB, LexTrans (lexical translation model, also disc training), CSplit (compound splitting), Tagger (sequence labeling), LexAlign (alignment only, or EM training)")
        ("input,i",po::value<string>()->default_value("-"),"Source file")
        ("grammar,g",po::value<vector<string> >()->composing(),"Either SCFG grammar file(s) or phrase tables file(s)")
        ("per_sentence_grammar_file", po::value<string>(), "Optional (and possibly not implemented) per sentence grammar file enables all per sentence grammars to be stored in a single large file and accessed by offset")
        ("weights,w",po::value<string>(),"Feature weights file")
    ("prelm_weights",po::value<string>(),"Feature weights file for prelm_beam_prune.  Requires --weights.")
    ("prelm_copy_weights","use --weights as value for --prelm_weights.")
    ("prelm_feature_function",po::value<vector<string> >()->composing(),"Additional feature functions for prelm pass only (in addition to the 0-state subset of feature_function")
    ("keep_prelm_cube_order","DEPRECATED (always enabled).  when forest rescoring with final models, use the edge ordering from the prelm pruning features*weights.  only meaningful if --prelm_weights given.  UNTESTED but assume that cube pruning gives a sensible result, and that 'good' (as tuned for bleu w/ prelm features) edges come first.")
    ("warn_0_weight","Warn about any feature id that has a 0 weight (this is perfectly safe if you intend 0 weight, though)")
        ("freeze_feature_set,Z", "Freeze feature set after reading feature weights file")
        ("feature_function,F",po::value<vector<string> >()->composing(), "Additional feature function(s) (-L for list)")
        ("fsa_feature_function,A",po::value<vector<string> >()->composing(), "Additional FSA feature function(s) (-L for list)")
    ("apply_fsa_by",po::value<string>()->default_value("BU_CUBE"), "Method for applying fsa_feature_functions - BU_FULL BU_CUBE EARLEY") //+ApplyFsaBy::all_names()
        ("list_feature_functions,L","List available feature functions")
        ("add_pass_through_rules,P","Add rules to translate OOV words as themselves")
	("k_best,k",po::value<int>(),"Extract the k best derivations")
	("unique_k_best,r", "Unique k-best translation list")
        ("aligner,a", "Run as a word/phrase aligner (src & ref required)")
        ("aligner_use_viterbi", "If run in alignment mode, compute the Viterbi (rather than MAP) alignment")
        ("intersection_strategy,I",po::value<string>()->default_value("cube_pruning"), "Intersection strategy for incorporating finite-state features; values include Cube_pruning, Full")
        ("cubepruning_pop_limit,K",po::value<int>()->default_value(200), "Max number of pops from the candidate heap at each node")
        ("goal",po::value<string>()->default_value("S"),"Goal symbol (SCFG & FST)")
        ("scfg_extra_glue_grammar", po::value<string>(), "Extra glue grammar file (Glue grammars apply when i=0 but have no other span restrictions)")
        ("scfg_no_hiero_glue_grammar,n", "No Hiero glue grammar (nb. by default the SCFG decoder adds Hiero glue rules)")
        ("scfg_default_nt,d",po::value<string>()->default_value("X"),"Default non-terminal symbol in SCFG")
        ("scfg_max_span_limit,S",po::value<int>()->default_value(10),"Maximum non-terminal span limit (except \"glue\" grammar)")
    ("quiet", "Disable verbose output")
    ("show_config", po::bool_switch(&show_config), "show contents of loaded -c config files.")
    ("show_weights", po::bool_switch(&show_weights), "show effective feature weights")
        ("show_joshua_visualization,J", "Produce output compatible with the Joshua visualization tools")
        ("show_tree_structure", "Show the Viterbi derivation structure")
        ("show_expected_length", "Show the expected translation length under the model")
        ("show_partition,z", "Compute and show the partition (inside score)")
        ("show_conditional_prob", "Output the conditional log prob to STDOUT instead of a translation")
        ("show_cfg_search_space", "Show the search space as a CFG")
    ("show_features","Show the feature vector for the viterbi translation")
    ("prelm_density_prune", po::value<double>(), "Applied to -LM forest just before final LM rescoring: keep no more than this many times the number of edges used in the best derivation tree (>=1.0)")
    ("density_prune", po::value<double>(), "Keep no more than this many times the number of edges used in the best derivation tree (>=1.0)")
        ("prelm_beam_prune", po::value<double>(), "Prune paths from -LM forest before LM rescoring, keeping paths within exp(alpha>=0)")
        ("coarse_to_fine_beam_prune", po::value<double>(), "Prune paths from coarse parse forest before fine parse, keeping paths within exp(alpha>=0)")
        ("ctf_beam_widen", po::value<double>()->default_value(2.0), "Expand coarse pass beam by this factor if no fine parse is found")
        ("ctf_num_widenings", po::value<int>()->default_value(2), "Widen coarse beam this many times before backing off to full parse")
        ("ctf_no_exhaustive", "Do not fall back to exhaustive parse if coarse-to-fine parsing fails")
        ("beam_prune", po::value<double>(), "Prune paths from +LM forest, keep paths within exp(alpha>=0)")
    ("scale_prune_srclen", "scale beams by the input length (in # of tokens; may not be what you want for lattices")
    ("promise_power",po::value<double>()->default_value(0), "Give more beam budget to more promising previous-pass nodes when pruning - but allocate the same average beams.  0 means off, 1 means beam proportional to inside*outside prob, n means nth power (affects just --cubepruning_pop_limit).  note: for the same pop_limit, this gives more search error unless very close to 0 (recommend disabled; even 0.01 is slightly worse than 0) which is a bad sign and suggests this isn't doing a good job; further it's slightly slower to LM cube rescore with 0.01 compared to 0, as well as giving (very insignificantly) lower BLEU.  TODO: test under more conditions, or try idea with different formula, or prob. cube beams.")
        ("lextrans_use_null", "Support source-side null words in lexical translation")
        ("lextrans_align_only", "Only used in alignment mode. Limit target words generated by reference")
        ("tagger_tagset,t", po::value<string>(), "(Tagger) file containing tag set")
        ("csplit_output_plf", "(Compound splitter) Output lattice in PLF format")
        ("csplit_preserve_full_word", "(Compound splitter) Always include the unsegmented form in the output lattice")
        ("extract_rules", po::value<string>(), "Extract the rules used in translation (de-duped) to this file")
        ("graphviz","Show (constrained) translation forest in GraphViz format")
        ("max_translation_beam,x", po::value<int>(), "Beam approximation to get max translation from the chart")
        ("max_translation_sample,X", po::value<int>(), "Sample the max translation from the chart")
        ("pb_max_distortion,D", po::value<int>()->default_value(4), "Phrase-based decoder: maximum distortion")
        ("cll_gradient,G","Compute conditional log-likelihood gradient and write to STDOUT (src & ref required)")
        ("crf_uniform_empirical", "If there are multple references use (i.e., lattice) a uniform distribution rather than posterior weighting a la EM")
    ("get_oracle_forest,o", "Calculate rescored hypregraph using approximate BLEU scoring of rules")
    ("feature_expectations","Write feature expectations for all features in chart (**OBJ** will be the partition)")
        ("vector_format",po::value<string>()->default_value("b64"), "Sparse vector serialization format for feature expectations or gradients, includes (text or b64)")
        ("combine_size,C",po::value<int>()->default_value(1), "When option -G is used, process this many sentence pairs before writing the gradient (1=emit after every sentence pair)")
        ("forest_output,O",po::value<string>(),"Directory to write forests to")
        ("minimal_forests,m","Write minimal forests (excludes Rule information). Such forests can be used for ML/MAP training, but not rescoring, etc.");
  // ob.AddOptions(&opts);
  po::options_description cfgo(cfg_options.description());
  cfg_options.AddOptions(&cfgo);
  po::options_description clo("Command line options");
  clo.add_options()
    ("config,c", po::value<vector<string> >(&cfg_files), "Configuration file(s) - latest has priority")
        ("help,h", "Print this help message and exit")
    ("usage,u", po::value<string>(), "Describe a feature function type")
    ("compgen", "Print just option names suitable for bash command line completion builtin 'compgen'")
    ;

  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts).add(cfgo);
  //add(opts).add(cfgo)
  dcmdline_options.add(dconfig_options).add(clo);
  if (argc) {
    argv_minus_to_underscore(argc,argv);
    po::store(parse_command_line(argc, argv, dcmdline_options), conf);
    if (conf.count("compgen")) {
      print_options(cout,dcmdline_options);
      cout << endl;
      exit(0);
    }
    ShowBanner();
  }
  if (conf.count("show_config")) // special handling needed because we only want to notify() once.
    show_config=true;
  if (conf.count("config") && !cfg) {
    typedef vector<string> Cs;
    Cs cs=conf["config"].as<Cs>();
    for (int i=0;i<cs.size();++i) {
      string cfg=cs[i];
      cerr << "Configuration file: " << cfg << endl;
      ReadFile conff(cfg);
      po::store(po::parse_config_file(*conff, dconfig_options), conf);
    }
  }
  if (cfg) po::store(po::parse_config_file(*cfg, dconfig_options), conf);
  po::notify(conf);
  if (show_config && !cfg_files.empty()) {
    cerr<< "\nConfig files:\n\n";
    for (int i=0;i<cfg_files.size();++i) {
      string cfg=cfg_files[i];
      cerr << "Configuration file: " << cfg << endl;
      CopyFile(cfg,cerr);
      cerr << "(end config "<<cfg<<"\n\n";
    }
    cerr <<"Command line:";
    for (int i=0;i<argc;++i)
      cerr<<" "<<argv[i];
    cerr << "\n\n";
  }
  if (conf.count("quiet"))
    SetSilent(true);

  if (conf.count("list_feature_functions")) {
    cerr << "Available feature functions (specify with -F; describe with -u FeatureName):\n";
    ff_registry.DisplayList(); //TODO
    cerr << "Available FSA feature functions (specify with --fsa_feature_function):\n";
    fsa_ff_registry.DisplayList(); // TODO
    cerr << endl;
    exit(1);
  }

  if (conf.count("usage")) {
    ff_usage(str("usage",conf));
    exit(0);
  }
  if (conf.count("help")) {
    cout << dcmdline_options << endl;
    exit(0);
  }
  if (conf.count("help") || conf.count("formalism") == 0) {
    cerr << dcmdline_options << endl;
    exit(1);
  }

  formalism = LowercaseString(str("formalism",conf));
  if (formalism != "scfg" && formalism != "fst" && formalism != "lextrans" && formalism != "pb" && formalism != "csplit" && formalism != "tagger" && formalism != "lexalign") {
    cerr << "Error: --formalism takes only 'scfg', 'fst', 'pb', 'csplit', 'lextrans', 'lexalign', or 'tagger'\n";
    cerr << dcmdline_options << endl;
    exit(1);
  }


  write_gradient = conf.count("cll_gradient");
  feature_expectations = conf.count("feature_expectations");
  if (write_gradient && feature_expectations) {
    cerr << "You can only specify --gradient or --feature_expectations, not both!\n";
    exit(1);
  }
  output_training_vector = (write_gradient || feature_expectations);

  const string formalism = LowercaseString(str("formalism",conf));
  const bool csplit_preserve_full_word = conf.count("csplit_preserve_full_word");
  if (csplit_preserve_full_word &&
      (formalism != "csplit" || !(conf.count("beam_prune")||conf.count("density_prune")||conf.count("prelm_beam_prune")||conf.count("prelm_density_prune")))) {
    cerr << "--csplit_preserve_full_word should only be "
         << "used with csplit AND --*_prune!\n";
    exit(1);
  }
  csplit_output_plf = conf.count("csplit_output_plf");
  if (csplit_output_plf && formalism != "csplit") {
    cerr << "--csplit_output_plf should only be used with csplit!\n";
    exit(1);
  }

  // load feature weights (and possibly freeze feature set)
  has_prelm_models = false;
  if (conf.count("weights")) {
    w.InitFromFile(str("weights",conf));
    feature_weights.resize(FD::NumFeats());
    w.InitVector(&feature_weights);
    string plmw;
    if (prelm_weights_string(conf,plmw)) {
      has_prelm_models = true;
      prelm_w.InitFromFile(plmw);
      prelm_feature_weights.resize(FD::NumFeats());
      prelm_w.InitVector(&prelm_feature_weights);
      if (show_weights)
        cerr << "prelm_weights: " << WeightVector(prelm_feature_weights)<<endl;
    }
    if (show_weights)
      cerr << "+LM weights: " << WeightVector(feature_weights)<<endl;
  }
  bool warn0=conf.count("warn_0_weight");
  bool freeze=conf.count("freeze_feature_set");
  bool early_freeze=freeze && !warn0;
  bool late_freeze=freeze && warn0;
  if (early_freeze) {
    cerr << "Freezing feature set" << endl;
    FD::Freeze(); // this means we can't see the feature names of not-weighted features
  }

  // set up translation back end
  if (formalism == "scfg")
    translator.reset(new SCFGTranslator(conf));
  else if (formalism == "fst")
    translator.reset(new FSTTranslator(conf));
  else if (formalism == "pb")
    translator.reset(new PhraseBasedTranslator(conf));
  else if (formalism == "csplit")
    translator.reset(new CompoundSplit(conf));
  else if (formalism == "lextrans")
    translator.reset(new LexicalTrans(conf));
  else if (formalism == "lexalign")
    translator.reset(new LexicalAlign(conf));
  else if (formalism == "tagger")
    translator.reset(new Tagger(conf));
  else
    assert(!"error");

  // set up additional scoring features
  if (conf.count("feature_function") > 0) {
    vector<string> add_ffs;
//    const vector<string>& add_ffs = conf["feature_function"].as<vector<string> >();
    store_conf(conf,"feature_function",&add_ffs);
    for (int i = 0; i < add_ffs.size(); ++i) {
      pffs.push_back(make_ff(add_ffs[i],verbose_feature_functions));
      FeatureFunction const* p=pffs.back().get();
      late_ffs.push_back(p);
      if (has_prelm_models) {
        if (p->NumBytesContext()==0)
          prelm_ffs.push_back(p);
        else
          cerr << "Excluding stateful feature from prelm pruning: "<<add_ffs[i]<<endl;
      }
    }
  }
  if (conf.count("prelm_feature_function") > 0) {
    vector<string> add_ffs;
    store_conf(conf,"prelm_feature_function",&add_ffs);
//    const vector<string>& add_ffs = conf["prelm_feature_function"].as<vector<string> >();
    for (int i = 0; i < add_ffs.size(); ++i) {
      prelm_only_ffs.push_back(make_ff(add_ffs[i],verbose_feature_functions,"prelm-only "));
      prelm_ffs.push_back(prelm_only_ffs.back().get());
    }
  }

  store_conf(conf,"fsa_feature_function",&fsa_names);
  for (int i=0;i<fsa_names.size();++i)
    fsa_ffs.push_back(make_fsa_ff(fsa_names[i],verbose_feature_functions,"FSA "));
  if (fsa_ffs.size()>1) {
    //FIXME: support N fsa ffs.
    cerr<<"Only the first fsa FF will be used (FIXME).\n";
    fsa_ffs.resize(1);
  }
  if (!fsa_ffs.empty()) {
    cerr<<"FSA: ";
    show_all_features(fsa_ffs,feature_weights,cerr,cerr,true,true);
  }

  if (late_freeze) {
    cerr << "Late freezing feature set (use --no_freeze_feature_set to prevent)." << endl;
    FD::Freeze(); // this means we can't see the feature names of not-weighted features
  }

  if (has_prelm_models)
        cerr << "prelm rescoring with "<<prelm_ffs.size()<<" 0-state feature functions.  +LM pass will use "<<late_ffs.size()<<" features (not counting rule features)."<<endl;

  late_models = new ModelSet(feature_weights, late_ffs);
  if (!SILENT) show_models(conf,*late_models,"late ");
  prelm_models = new ModelSet(prelm_feature_weights, prelm_ffs);
  if (has_prelm_models) {
    if (!SILENT) show_models(conf,*prelm_models,"prelm "); }

  int palg = 1;
  if (LowercaseString(str("intersection_strategy",conf)) == "full") {
    palg = 0;
    cerr << "Using full intersection (no pruning).\n";
  }
  pop_limit=conf["cubepruning_pop_limit"].as<int>();
  inter_conf = new IntersectionConfiguration(palg, pop_limit);

  sample_max_trans = conf.count("max_translation_sample") ?
    conf["max_translation_sample"].as<int>() : 0;
  if (sample_max_trans)
    rng.reset(new RandomNumberGenerator<boost::mt19937>);
  aligner_mode = conf.count("aligner");
  minimal_forests = conf.count("minimal_forests");
  graphviz = conf.count("graphviz");
  joshua_viz = conf.count("show_joshua_visualization");
  encode_b64 = str("vector_format",conf) == "b64";
  kbest = conf.count("k_best");
  unique_kbest = conf.count("unique_k_best");
  crf_uniform_empirical = conf.count("crf_uniform_empirical");
  get_oracle_forest = conf.count("get_oracle_forest");

  cfg_options.Validate();

  if (conf.count("extract_rules"))
    extract_file.reset(new WriteFile(str("extract_rules",conf)));

  combine_size = conf["combine_size"].as<int>();
  if (combine_size < 1) combine_size = 1;
  sent_id = -1;
  acc_obj = 0; // accumulate objective
  g_count = 0;    // number of gradient pieces computed
}

Decoder::Decoder(istream* cfg) { pimpl_.reset(new DecoderImpl(conf,0,0,cfg)); }
Decoder::Decoder(int argc, char** argv) { pimpl_.reset(new DecoderImpl(conf,argc, argv, 0)); }
Decoder::~Decoder() {}
void Decoder::SetId(int next_sent_id) { pimpl_->SetId(next_sent_id); }
bool Decoder::Decode(const string& input, DecoderObserver* o) {
  bool del = false;
  if (!o) { o = new DecoderObserver; del = true; }
  const bool res = pimpl_->Decode(input, o);
  if (del) delete o;
  return res;
}
void Decoder::SetWeights(const vector<double>& weights) { pimpl_->SetWeights(weights); }


bool DecoderImpl::Decode(const string& input, DecoderObserver* o) {
  string buf = input;
  NgramCache::Clear();   // clear ngram cache for remote LM (if used)
  Timer::Summarize();
  ++sent_id;
  map<string, string> sgml;
  ProcessAndStripSGML(&buf, &sgml);
  if (sgml.find("id") != sgml.end())
    sent_id = atoi(sgml["id"].c_str());

  if (!SILENT) {
    cerr << "\nINPUT: ";
    if (buf.size() < 100)
      cerr << buf << endl;
    else {
      size_t x = buf.rfind(" ", 100);
      if (x == string::npos) x = 100;
      cerr << buf.substr(0, x) << " ..." << endl;
    }
    cerr << "  id = " << sent_id << endl;
  }
  string to_translate;
  Lattice ref;
  ParseTranslatorInputLattice(buf, &to_translate, &ref);
  const unsigned srclen=NTokens(to_translate,' ');
//FIXME: should get the avg. or max source length of the input lattice (like Lattice::dist_(start,end)); but this is only used to scale beam parameters (optionally) anyway so fidelity isn't important.
  const bool has_ref = ref.size() > 0;
  SentenceMetadata smeta(sent_id, ref);
  smeta.sgml_.swap(sgml);
  o->NotifyDecodingStart(smeta);
  Hypergraph forest;          // -LM forest
  translator->ProcessMarkupHints(smeta.sgml_);
  Timer t("Translation");
  const bool translation_successful =
    translator->Translate(to_translate, &smeta, feature_weights, &forest);
  //TODO: modify translator to incorporate all 0-state model scores immediately?
  translator->SentenceComplete();

  if (!translation_successful) {
    if (!SILENT) cerr << "  NO PARSE FOUND.\n";
    o->NotifySourceParseFailure(smeta);
    o->NotifyDecodingComplete(smeta);
    if (conf.count("show_conditional_prob")) {
      cout << "-Inf" << endl << flush;
    }
    return false;
  }

  const bool show_tree_structure=conf.count("show_tree_structure");
  const bool show_features=conf.count("show_features");
  if (!SILENT) forest_stats(forest,"  -LM forest",show_tree_structure,show_features,feature_weights,oracle.show_derivation);
  if (conf.count("show_expected_length")) {
    const PRPair<double, double> res =
      Inside<PRPair<double, double>,
             PRWeightFunction<double, EdgeProb, double, ELengthWeightFunction> >(forest);
    cerr << "  Expected length  (words): " << res.r / res.p << "\t" << res << endl;
  }
  if (conf.count("show_partition")) {
    const prob_t z = Inside<prob_t, EdgeProb>(forest);
    cerr << "  -LM partition     log(Z): " << log(z) << endl;
  }

  if (has_prelm_models) {
    Timer t("prelm rescoring");
    forest.Reweight(prelm_feature_weights);
    Hypergraph prelm_forest;
    prelm_models->PrepareForInput(smeta);
    ApplyModelSet(forest,
                  smeta,
                  *prelm_models,
                  *inter_conf, // this is now reduced to exhaustive if all are stateless
                  &prelm_forest);
    forest.swap(prelm_forest);
    forest.Reweight(prelm_feature_weights); //FIXME: why the reweighting? here and below.  maybe in case we already had a featval for that id and ApplyModelSet only adds prob, doesn't recompute it?
    forest_stats(forest," prelm forest",show_tree_structure,show_features,prelm_feature_weights,oracle.show_derivation);
  }

  maybe_prune(forest,conf,"prelm_beam_prune","prelm_density_prune","-LM",srclen);

  cfg_options.maybe_output_source(forest);

  bool has_late_models = !late_models->empty();
  if (has_late_models) {
    Timer t("Forest rescoring:");
    late_models->PrepareForInput(smeta);
    forest.Reweight(feature_weights);
    Hypergraph lm_forest;
    ApplyModelSet(forest,
                  smeta,
                  *late_models,
                  *inter_conf,
                  &lm_forest);
    forest.swap(lm_forest);
    forest.Reweight(feature_weights);
    if (!SILENT) forest_stats(forest,"  +LM forest",show_tree_structure,show_features,feature_weights,oracle.show_derivation);
  }

  maybe_prune(forest,conf,"beam_prune","density_prune","+LM",srclen);

  HgCFG hgcfg(forest);
  cfg_options.prepare(hgcfg);

  if (!fsa_ffs.empty()) {
    Timer t("Target FSA rescoring:");
    if (!has_late_models)
      forest.Reweight(feature_weights);
    Hypergraph fsa_forest;
    assert(fsa_ffs.size()==1);
    ApplyFsaBy cfg(str("apply_fsa_by",conf),pop_limit);
    if (!SILENT) cerr << "FSA rescoring with "<<cfg<<" "<<fsa_ffs[0]->describe()<<endl;
    ApplyFsaModels(hgcfg,smeta,*fsa_ffs[0],feature_weights,cfg,&fsa_forest);
    forest.swap(fsa_forest);
    forest.Reweight(feature_weights);
    if (!SILENT) forest_stats(forest,"  +FSA forest",show_tree_structure,show_features,feature_weights,oracle.show_derivation);
  }

  // Oracle Rescoring
  if(get_oracle_forest) {
    Oracle oc=oracle.ComputeOracle(smeta,&forest,FeatureVector(feature_weights),10,conf["forest_output"].as<std::string>());
    if (!SILENT) cerr << "  +Oracle BLEU forest (nodes/edges): " << forest.nodes_.size() << '/' << forest.edges_.size() << endl;
    if (!SILENT) cerr << "  +Oracle BLEU (paths): " << forest.NumberOfPaths() << endl;
    oc.hope.Print(cerr,"  +Oracle BLEU");
    oc.fear.Print(cerr,"  -Oracle BLEU");
    //Add 1-best translation (trans) to psuedo-doc vectors
    if (!SILENT) oracle.IncludeLastScore(&cerr);
  }
  o->NotifyTranslationForest(smeta, &forest);

  // TODO I think this should probably be handled by an Observer
  if (conf.count("forest_output") && !has_ref) {
    ForestWriter writer(str("forest_output",conf), sent_id);
    if (FileExists(writer.fname_)) {
      if (!SILENT) cerr << "  Unioning...\n";
      Hypergraph new_hg;
      {
        ReadFile rf(writer.fname_);
        bool succeeded = HypergraphIO::ReadFromJSON(rf.stream(), &new_hg);
        assert(succeeded);
      }
      new_hg.Union(forest);
      bool succeeded = writer.Write(new_hg, minimal_forests);
      assert(succeeded);
    } else {
      bool succeeded = writer.Write(forest, minimal_forests);
      assert(succeeded);
    }
  }

  // TODO I think this should probably be handled by an Observer
  if (sample_max_trans) {
    MaxTranslationSample(&forest, sample_max_trans, conf.count("k_best") ? conf["k_best"].as<int>() : 0);
  } else {
    if (kbest && !has_ref) {
      //TODO: does this work properly?
      oracle.DumpKBest(sent_id, forest, conf["k_best"].as<int>(), unique_kbest,"-");
    } else if (csplit_output_plf) {
      cout << HypergraphIO::AsPLF(forest, false) << endl;
    } else {
      if (!graphviz && !has_ref && !joshua_viz) {
        vector<WordID> trans;
        ViterbiESentence(forest, &trans);
        cout << TD::GetString(trans) << endl << flush;
      }
      if (joshua_viz) {
        cout << sent_id << " ||| " << JoshuaVisualizationString(forest) << " ||| 1.0 ||| " << -1.0 << endl << flush;
      }
    }
  }

  prob_t first_z;
  if (conf.count("show_conditional_prob")) {
    first_z = Inside<prob_t, EdgeProb>(forest);
  }

  // TODO this should be handled by an Observer
  const int max_trans_beam_size = conf.count("max_translation_beam") ?
    conf["max_translation_beam"].as<int>() : 0;
  if (max_trans_beam_size) {
    Hack::MaxTrans(forest, max_trans_beam_size);
    return true;
  }

  // TODO this should be handled by an Observer
  if (graphviz && !has_ref) forest.PrintGraphviz();

  // the following are only used if write_gradient is true!
  SparseVector<prob_t> full_exp, ref_exp, gradient;
  double log_z = 0, log_ref_z = 0;
  if (write_gradient) {
    const prob_t z = InsideOutside<prob_t, EdgeProb, SparseVector<prob_t>, EdgeFeaturesAndProbWeightFunction>(forest, &full_exp);
    log_z = log(z);
    full_exp /= z;
  }
  if (conf.count("show_cfg_search_space"))
    HypergraphIO::WriteAsCFG(forest);
  if (has_ref) {
    if (HG::Intersect(ref, &forest)) {
      if (!SILENT) forest_stats(forest,"  Constr. forest",show_tree_structure,show_features,feature_weights,oracle.show_derivation);
      if (crf_uniform_empirical) {
        if (!SILENT) cerr << "  USING UNIFORM WEIGHTS\n";
        for (int i = 0; i < forest.edges_.size(); ++i)
          forest.edges_[i].edge_prob_=prob_t::One();
      } else {
        forest.Reweight(feature_weights);
        if (!SILENT) cerr << "  Constr. VitTree: " << ViterbiFTree(forest) << endl;
      }
      if (conf.count("show_partition")) {
         const prob_t z = Inside<prob_t, EdgeProb>(forest);
         cerr << "  Contst. partition  log(Z): " << log(z) << endl;
      }
      o->NotifyAlignmentForest(smeta, &forest);
      if (conf.count("forest_output")) {
        ForestWriter writer(str("forest_output",conf), sent_id);
        if (FileExists(writer.fname_)) {
          if (!SILENT) cerr << "  Unioning...\n";
          Hypergraph new_hg;
          {
            ReadFile rf(writer.fname_);
            bool succeeded = HypergraphIO::ReadFromJSON(rf.stream(), &new_hg);
            assert(succeeded);
          }
          new_hg.Union(forest);
          bool succeeded = writer.Write(new_hg, minimal_forests);
          assert(succeeded);
        } else {
          bool succeeded = writer.Write(forest, minimal_forests);
          assert(succeeded);
        }
      }
      if (aligner_mode && !output_training_vector)
        AlignerTools::WriteAlignment(smeta.GetSourceLattice(), smeta.GetReference(), forest, &cout, 0 == conf.count("aligner_use_viterbi"));
      if (write_gradient) {
        const prob_t ref_z = InsideOutside<prob_t, EdgeProb, SparseVector<prob_t>, EdgeFeaturesAndProbWeightFunction>(forest, &ref_exp);
        ref_exp /= ref_z;
        if (crf_uniform_empirical) {
          log_ref_z = ref_exp.dot(feature_weights);
        } else {
          log_ref_z = log(ref_z);
        }
        //cerr << "      MODEL LOG Z: " << log_z << endl;
        //cerr << "  EMPIRICAL LOG Z: " << log_ref_z << endl;
        if ((log_z - log_ref_z) < kMINUS_EPSILON) {
          cerr << "DIFF. ERR! log_z < log_ref_z: " << log_z << " " << log_ref_z << endl;
          exit(1);
        }
        assert(!isnan(log_ref_z));
        ref_exp -= full_exp;
        acc_vec += ref_exp;
        acc_obj += (log_z - log_ref_z);
      }
      if (feature_expectations) {
        const prob_t z =
          InsideOutside<prob_t, EdgeProb, SparseVector<prob_t>, EdgeFeaturesAndProbWeightFunction>(forest, &ref_exp);
        ref_exp /= z;
        acc_obj += log(z);
        acc_vec += ref_exp;
      }

      if (output_training_vector) {
        acc_vec.erase(0);
        ++g_count;
        if (g_count % combine_size == 0) {
          if (encode_b64) {
            cout << "0\t";
            SparseVector<double> dav; ConvertSV(acc_vec, &dav);
            B64::Encode(acc_obj, dav, &cout);
            cout << endl << flush;
          } else {
            cout << "0\t**OBJ**=" << acc_obj << ';' <<  acc_vec << endl << flush;
          }
          acc_vec.clear();
          acc_obj = 0;
        }
      }
      if (conf.count("graphviz")) forest.PrintGraphviz();
      if (kbest)
        oracle.DumpKBest(sent_id, forest, conf["k_best"].as<int>(), unique_kbest,"-");
      if (conf.count("show_conditional_prob")) {
        const prob_t ref_z = Inside<prob_t, EdgeProb>(forest);
        cout << (log(ref_z) - log(first_z)) << endl << flush;
      }
    } else {
      o->NotifyAlignmentFailure(smeta);
      if (!SILENT) cerr << "  REFERENCE UNREACHABLE.\n";
      if (write_gradient) {
        cout << endl << flush;
      }
      if (conf.count("show_conditional_prob")) {
        cout << "-Inf" << endl << flush;
      }
    }
  }
  o->NotifyDecodingComplete(smeta);
  return true;
}

