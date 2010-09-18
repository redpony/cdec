#include "decoder.h"

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "ff_factory.h"
#include "cfg_options.h"
#include "stringlib.h"
#include "hg.h"

using namespace std;
using boost::shared_ptr;
namespace po = boost::program_options;

inline void ShowBanner() {
  cerr << "cdec v1.0 (c) 2009-2010 by Chris Dyer\n";
}

inline string str(char const* name,po::variables_map const& conf) {
  return conf[name].as<string>();
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

struct DecoderImpl {
  DecoderImpl(int argc, char** argv, istream* cfg);
  bool Decode(const string& input) {
    return false;
  }
  bool DecodeProduceHypergraph(const string& input, Hypergraph* hg) {
    return false;
  }
  void SetWeights(const vector<double>& weights) {
  }

  po::variables_map conf;
  CFGOptions cfg_options;
};

DecoderImpl::DecoderImpl(int argc, char** argv, istream* cfg) {
  if (cfg) { if (argc || argv) { cerr << "DecoderImpl() can only take a file or command line options, not both\n"; exit(1); } }
  bool show_config;
  bool show_weights;
  vector<string> cfg_files;

  po::options_description opts("Configuration options");
  opts.add_options()
        ("formalism,f",po::value<string>(),"Decoding formalism; values include SCFG, FST, PB, LexTrans (lexical translation model, also disc training), CSplit (compound splitting), Tagger (sequence labeling), LexAlign (alignment only, or EM training)")
        ("input,i",po::value<string>()->default_value("-"),"Source file")
        ("grammar,g",po::value<vector<string> >()->composing(),"Either SCFG grammar file(s) or phrase tables file(s)")
        ("weights,w",po::value<string>(),"Feature weights file")
    ("prelm_weights",po::value<string>(),"Feature weights file for prelm_beam_prune.  Requires --weights.")
    ("prelm_copy_weights","use --weights as value for --prelm_weights.")
    ("prelm_feature_function",po::value<vector<string> >()->composing(),"Additional feature functions for prelm pass only (in addition to the 0-state subset of feature_function")
    ("keep_prelm_cube_order","DEPRECATED (always enabled).  when forest rescoring with final models, use the edge ordering from the prelm pruning features*weights.  only meaningful if --prelm_weights given.  UNTESTED but assume that cube pruning gives a sensible result, and that 'good' (as tuned for bleu w/ prelm features) edges come first.")
    ("warn_0_weight","Warn about any feature id that has a 0 weight (this is perfectly safe if you intend 0 weight, though)")
        ("no_freeze_feature_set,Z", "Do not freeze feature set after reading feature weights file")
        ("feature_function,F",po::value<vector<string> >()->composing(), "Additional feature function(s) (-L for list)")
        ("fsa_feature_function,A",po::value<vector<string> >()->composing(), "Additional FSA feature function(s) (-L for list)")
    ("apply_fsa_by",po::value<string>()->default_value("BU_CUBE"), "Method for applying fsa_feature_functions - BU_FULL BU_CUBE EARLEY") //+ApplyFsaBy::all_names()
        ("list_feature_functions,L","List available feature functions")
        ("add_pass_through_rules,P","Add rules to translate OOV words as themselves")
	("k_best,k",po::value<int>(),"Extract the k best derivations")
	("unique_k_best,r", "Unique k-best translation list")
        ("aligner,a", "Run as a word/phrase aligner (src & ref required)")
        ("intersection_strategy,I",po::value<string>()->default_value("cube_pruning"), "Intersection strategy for incorporating finite-state features; values include Cube_pruning, Full")
        ("cubepruning_pop_limit,K",po::value<int>()->default_value(200), "Max number of pops from the candidate heap at each node")
        ("goal",po::value<string>()->default_value("S"),"Goal symbol (SCFG & FST)")
        ("scfg_extra_glue_grammar", po::value<string>(), "Extra glue grammar file (Glue grammars apply when i=0 but have no other span restrictions)")
        ("scfg_no_hiero_glue_grammar,n", "No Hiero glue grammar (nb. by default the SCFG decoder adds Hiero glue rules)")
        ("scfg_default_nt,d",po::value<string>()->default_value("X"),"Default non-terminal symbol in SCFG")
        ("scfg_max_span_limit,S",po::value<int>()->default_value(10),"Maximum non-terminal span limit (except \"glue\" grammar)")
    ("show_config", po::bool_switch(&show_config), "show contents of loaded -c config files.")
    ("show_weights", po::bool_switch(&show_weights), "show effective feature weights")
        ("show_joshua_visualization,J", "Produce output compatible with the Joshua visualization tools")
        ("show_tree_structure", "Show the Viterbi derivation structure")
        ("show_expected_length", "Show the expected translation length under the model")
        ("show_partition,z", "Compute and show the partition (inside score)")
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
        ("lexalign_use_null", "Support source-side null words in lexical translation")
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
  if (conf.count("config") || cfg) {
    typedef vector<string> Cs;
    Cs cs=conf["config"].as<Cs>();
    for (int i=0;i<cs.size();++i) {
      string cfg=cs[i];
      cerr << "Configuration file: " << cfg << endl;
      ReadFile conff(cfg);
      po::store(po::parse_config_file(*conff, dconfig_options), conf);
    }
    if (cfg) po::store(po::parse_config_file(*cfg, dconfig_options), conf);
  }
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


  if (conf.count("list_feature_functions")) {
    cerr << "Available feature functions (specify with -F; describe with -u FeatureName):\n";
    // ff_registry.DisplayList(); //TODO
    cerr << "Available FSA feature functions (specify with --fsa_feature_function):\n";
   // fsa_ff_registry.DisplayList(); // TODO
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

  const string formalism = LowercaseString(str("formalism",conf));
  if (formalism != "scfg" && formalism != "fst" && formalism != "lextrans" && formalism != "pb" && formalism != "csplit" && formalism != "tagger" && formalism != "lexalign") {
    cerr << "Error: --formalism takes only 'scfg', 'fst', 'pb', 'csplit', 'lextrans', 'lexalign', or 'tagger'\n";
    cerr << dcmdline_options << endl;
    exit(1);
  }

}

Decoder::Decoder(istream* cfg) {
  pimpl_.reset(new DecoderImpl(0,0,cfg));
}

Decoder::Decoder(int argc, char** argv) {
  pimpl_.reset(new DecoderImpl(argc, argv, 0));
}

Decoder::~Decoder() {}

bool Decoder::Decode(const string& input) {
  return pimpl_->Decode(input);
}

bool Decoder::DecodeProduceHypergraph(const string& input, Hypergraph* hg) {
  return pimpl_->DecodeProduceHypergraph(input, hg);
}

void Decoder::SetWeights(const vector<double>& weights) {
  pimpl_->SetWeights(weights);
}

void InitCommandLine(int argc, char** argv, po::variables_map* confp) {
  po::variables_map &conf=*confp;

}
