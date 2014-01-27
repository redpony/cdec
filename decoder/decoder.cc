#include "decoder.h"

#ifndef HAVE_OLD_CPP
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std { using std::tr1::unordered_map; }
#endif
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>

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
#include "hg_union.h"

#include "oracle_bleu.h"
#include "apply_models.h"
#include "ff.h"
#include "ffset.h"
#include "ff_factory.h"
#include "viterbi.h"
#include "kbest.h"
#include "inside_outside.h"
#include "exp_semiring.h"
#include "sentence_metadata.h"
#include "sampler.h"

#include "forest_writer.h" // TODO this section should probably be handled by an Observer
#include "incremental.h"
#include "hg_io.h"
#include "aligner.h"

#ifdef CP_TIME
    clock_t CpTime::time_;
	void CpTime::Add(clock_t x){time_+=x;}
	void CpTime::Sub(clock_t x){time_-=x;}
	double CpTime::Get(){return (double)(time_)/CLOCKS_PER_SEC;}
#endif

static const double kMINUS_EPSILON = -1e-6;  // don't be too strict

using namespace std;
namespace po = boost::program_options;

static bool verbose_feature_functions=true;

namespace Hack { void MaxTrans(const Hypergraph& in, int beam_size); }
namespace NgramCache { void Clear(); }

DecoderObserver::~DecoderObserver() {}
void DecoderObserver::NotifyDecodingStart(const SentenceMetadata&) {}
void DecoderObserver::NotifySourceParseFailure(const SentenceMetadata&) {}
void DecoderObserver::NotifyTranslationForest(const SentenceMetadata&, Hypergraph*) {}
void DecoderObserver::NotifyAlignmentFailure(const SentenceMetadata&) {}
void DecoderObserver::NotifyAlignmentForest(const SentenceMetadata&, Hypergraph*) {}
void DecoderObserver::NotifyDecodingComplete(const SentenceMetadata&) {}

enum SummaryFeature {
  kNODE_RISK = 1,
  kEDGE_RISK,
  kEDGE_PROB
};


struct ELengthWeightFunction {
  double operator()(const Hypergraph::Edge& e) const {
    return e.rule_->ELength() - e.rule_->Arity();
  }
};
inline void ShowBanner() {
  cerr << "cdec (c) 2009--2014 by Chris Dyer\n";
}

inline string str(char const* name,po::variables_map const& conf) {
  return conf[name].as<string>();
}


// print just the --long_opt names suitable for bash compgen
inline void print_options(std::ostream &out,po::options_description const& opts) {
  typedef std::vector< boost::shared_ptr<po::option_description> > Ds;
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

inline boost::shared_ptr<FeatureFunction> make_ff(string const& ffp,bool verbose_feature_functions,char const* pre="") {
  string ff, param;
  SplitCommandAndParam(ffp, &ff, &param);
  if (verbose_feature_functions && !SILENT)
    cerr << pre << "feature: " << ff;
  if (!SILENT) {
    if (param.size() > 0) cerr << " (with config parameters '" << param << "')\n";
    else cerr << " (no config parameters)\n";
  }
  boost::shared_ptr<FeatureFunction> pf = ff_registry.Create(ff, param);
  if (!pf) exit(1);
  int nbyte=pf->StateSize();
  if (verbose_feature_functions && !SILENT)
    cerr<<"State is "<<nbyte<<" bytes for "<<pre<<"feature "<<ffp<<endl;
  return pf;
}

// when the translation forest is first built, it is scored by the features associated
// with the rules. To add other features (like language models, etc), cdec applies one or
// more "rescoring passes", which compute new features and optionally apply new weights
// and then prune the resulting (rescored) hypergraph. All feature values from previous
// passes are carried over into subsequent passes (where they may have different weights).
struct RescoringPass {
  RescoringPass() : fid_summary(), density_prune(), beam_prune() {}
  boost::shared_ptr<ModelSet> models;
  boost::shared_ptr<IntersectionConfiguration> inter_conf;
  vector<const FeatureFunction*> ffs;
  boost::shared_ptr<vector<weight_t> > weight_vector;
  int fid_summary;            // 0 == no summary feature
  double density_prune;       // 0 == don't density prune
  double beam_prune;          // 0 == don't beam prune
};

ostream& operator<<(ostream& os, const RescoringPass& rp) {
  os << "[num_fn=" << rp.ffs.size();
  if (rp.inter_conf) { os << " int_alg=" << *rp.inter_conf; }
  //if (rp.weight_vector.size() > 0) os << " new_weights";
  if (rp.fid_summary) os << " summary_feature=" << FD::Convert(rp.fid_summary);
  if (rp.density_prune) os << " density_prune=" << rp.density_prune;
  if (rp.beam_prune) os << " beam_prune=" << rp.beam_prune;
  os << ']';
  return os;
}

struct DecoderImpl {
  DecoderImpl(po::variables_map& conf, int argc, char** argv, istream* cfg);
  ~DecoderImpl();
  bool Decode(const string& input, DecoderObserver*);
  vector<weight_t>& CurrentWeightVector() {
    return (rescoring_passes.empty() ? *init_weights : *rescoring_passes.back().weight_vector);
  }
  void SetId(int next_sent_id) { sent_id = next_sent_id - 1; }

  void forest_stats(Hypergraph &forest,string name,bool show_tree,bool show_deriv=false, bool extract_rules=false, boost::shared_ptr<WriteFile> extract_file = boost::make_shared<WriteFile>()) {
    cerr << viterbi_stats(forest,name,true,show_tree,show_deriv,extract_rules, extract_file);
    cerr << endl;
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
      forest.PruneInsideOutside(beam_prune,density_prune,pm,false,1);
      if (!forestname.empty()) forestname=" "+forestname;
      if (!SILENT) { 
        forest_stats(forest,"  Pruned "+forestname+" forest",false,false);
        cerr << "  Pruned "<<forestname<<" forest portion of edges kept: "<<forest.edges_.size()/presize<<endl;
      }
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

  // used to construct the suffix string to get the name of arguments for multiple passes
  // e.g., the "2" in --weights2
  static string StringSuffixForRescoringPass(int pass) {
    if (pass == 0) return "";
    string ps = "1";
    assert(pass < 9);
    ps[0] += pass;
    return ps;
  }

  vector<RescoringPass> rescoring_passes;

  po::variables_map& conf;
  OracleBleu oracle;
  string formalism;
  boost::shared_ptr<Translator> translator;
  boost::shared_ptr<vector<weight_t> > init_weights; // weights used with initial parse
  vector<boost::shared_ptr<FeatureFunction> > pffs;
  boost::shared_ptr<RandomNumberGenerator<boost::mt19937> > rng;
  int sample_max_trans;
  bool aligner_mode;
  bool graphviz; 
  bool joshua_viz;
  bool encode_b64;
  bool kbest;
  bool unique_kbest;
  bool get_oracle_forest;
  boost::shared_ptr<WriteFile> extract_file;
  int combine_size;
  int sent_id;
  SparseVector<prob_t> acc_vec;  // accumulate gradient
  double acc_obj; // accumulate objective
  int g_count;    // number of gradient pieces computed
  bool csplit_output_plf;
  bool write_gradient; // TODO Observer
  bool feature_expectations; // TODO Observer
  bool output_training_vector; // TODO Observer
  bool remove_intersected_rule_annotations;
  boost::scoped_ptr<IncrementalBase> incremental;


  static void ConvertSV(const SparseVector<prob_t>& src, SparseVector<double>* trg) {
    for (SparseVector<prob_t>::const_iterator it = src.begin(); it != src.end(); ++it)
      trg->set_value(it->first, it->second.as_float());
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
        ("list_feature_functions,L","List available feature functions")
#ifdef HAVE_CMPH
        ("cmph_perfect_feature_hash,h", po::value<string>(), "Load perfect hash function for features")
#endif

        ("weights,w",po::value<string>(),"Feature weights file (initial forest / pass 1)")
        ("feature_function,F",po::value<vector<string> >()->composing(), "Pass 1 additional feature function(s) (-L for list)")
        ("intersection_strategy,I",po::value<string>()->default_value("cube_pruning"), "Pass 1 intersection strategy for incorporating finite-state features; values include Cube_pruning, Full, Fast_cube_pruning, Fast_cube_pruning_2")
        ("cubepruning_pop_limit,K",po::value<unsigned>()->default_value(200), "Max number of pops from the candidate heap at each node")
        ("summary_feature", po::value<string>(), "Compute a 'summary feature' at the end of the pass (before any pruning) with name=arg and value=inside-outside/Z")
        ("summary_feature_type", po::value<string>()->default_value("node_risk"), "Summary feature types: node_risk, edge_risk, edge_prob")
        ("density_prune", po::value<double>(), "Pass 1 pruning: keep no more than this many times the number of edges used in the best derivation tree (>=1.0)")
        ("beam_prune", po::value<double>(), "Pass 1 pruning: Prune paths from scored forest, keep paths within exp(alpha>=0)")

        ("weights2",po::value<string>(),"Optional pass 2")
        ("feature_function2",po::value<vector<string> >()->composing(), "Optional pass 2")
        ("intersection_strategy2",po::value<string>()->default_value("cube_pruning"), "Optional pass 2")
        ("cubepruning_pop_limit2",po::value<unsigned>()->default_value(200), "Optional pass 2")
        ("summary_feature2", po::value<string>(), "Optional pass 2")
        ("density_prune2", po::value<double>(), "Optional pass 2")
        ("beam_prune2", po::value<double>(), "Optional pass 2")

        ("weights3",po::value<string>(),"Optional pass 3")
        ("feature_function3",po::value<vector<string> >()->composing(), "Optional pass 3")
        ("intersection_strategy3",po::value<string>()->default_value("cube_pruning"), "Optional pass 3")
        ("cubepruning_pop_limit3",po::value<unsigned>()->default_value(200), "Optional pass 3")
        ("summary_feature3", po::value<string>(), "Optional pass 3")
        ("density_prune3", po::value<double>(), "Optional pass 3")
        ("beam_prune3", po::value<double>(), "Optional pass 3")

        ("add_pass_through_rules,P","Add rules to translate OOV words as themselves")
        ("k_best,k",po::value<int>(),"Extract the k best derivations")
        ("unique_k_best,r", "Unique k-best translation list")
        ("aligner,a", "Run as a word/phrase aligner (src & ref required)")
        ("aligner_use_viterbi", "If run in alignment mode, compute the Viterbi (rather than MAP) alignment")
        ("goal",po::value<string>()->default_value("S"),"Goal symbol (SCFG & FST)")
        ("freeze_feature_set,Z", "Freeze feature set after reading feature weights file")
        ("warn_0_weight","Warn about any feature id that has a 0 weight (this is perfectly safe if you intend 0 weight, though)")
        ("scfg_extra_glue_grammar", po::value<string>(), "Extra glue grammar file (Glue grammars apply when i=0 but have no other span restrictions)")
        ("scfg_no_hiero_glue_grammar,n", "No Hiero glue grammar (nb. by default the SCFG decoder adds Hiero glue rules)")
        ("scfg_default_nt,d",po::value<string>()->default_value("X"),"Default non-terminal symbol in SCFG")
        ("scfg_max_span_limit,S",po::value<int>()->default_value(10),"Maximum non-terminal span limit (except \"glue\" grammar)")
        ("quiet", "Disable verbose output")
        ("show_config", po::bool_switch(&show_config), "show contents of loaded -c config files.")
        ("show_weights", po::bool_switch(&show_weights), "show effective feature weights")
        ("show_feature_dictionary", "After decoding the last input, write the contents of the feature dictionary")
        ("show_joshua_visualization,J", "Produce output compatible with the Joshua visualization tools")
        ("show_tree_structure", "Show the Viterbi derivation structure")
        ("show_expected_length", "Show the expected translation length under the model")
        ("show_partition,z", "Compute and show the partition (inside score)")
        ("show_conditional_prob", "Output the conditional log prob to STDOUT instead of a translation")
        ("show_cfg_search_space", "Show the search space as a CFG")
        ("show_cfg_alignment_space", "Show the alignment hypergraph as a CFG")
        ("show_target_graph", po::value<string>(), "Directory to write the target hypergraphs to")
        ("incremental_search", po::value<string>(), "Run lazy search with this language model file")
        ("coarse_to_fine_beam_prune", po::value<double>(), "Prune paths from coarse parse forest before fine parse, keeping paths within exp(alpha>=0)")
        ("ctf_beam_widen", po::value<double>()->default_value(2.0), "Expand coarse pass beam by this factor if no fine parse is found")
        ("ctf_num_widenings", po::value<int>()->default_value(2), "Widen coarse beam this many times before backing off to full parse")
        ("ctf_no_exhaustive", "Do not fall back to exhaustive parse if coarse-to-fine parsing fails")
        ("scale_prune_srclen", "scale beams by the input length (in # of tokens; may not be what you want for lattices")
        ("lextrans_dynasearch", "'DynaSearch' neighborhood instead of usual partition, as defined by Smith & Eisner (2005)")
        ("lextrans_use_null", "Support source-side null words in lexical translation")
        ("lextrans_align_only", "Only used in alignment mode. Limit target words generated by reference")
        ("tagger_tagset,t", po::value<string>(), "(Tagger) file containing tag set")
        ("csplit_output_plf", "(Compound splitter) Output lattice in PLF format")
        ("csplit_preserve_full_word", "(Compound splitter) Always include the unsegmented form in the output lattice")
        ("extract_rules", po::value<string>(), "Extract the rules used in translation (not de-duped!) to a file in this directory")
        ("show_derivations", po::value<string>(), "Directory to print the derivation structures to")
        ("graphviz","Show (constrained) translation forest in GraphViz format")
        ("max_translation_beam,x", po::value<int>(), "Beam approximation to get max translation from the chart")
        ("max_translation_sample,X", po::value<int>(), "Sample the max translation from the chart")
        ("pb_max_distortion,D", po::value<int>()->default_value(4), "Phrase-based decoder: maximum distortion")
        ("cll_gradient,G","Compute conditional log-likelihood gradient and write to STDOUT (src & ref required)")
        ("get_oracle_forest,o", "Calculate rescored hypregraph using approximate BLEU scoring of rules")
        ("feature_expectations","Write feature expectations for all features in chart (**OBJ** will be the partition)")
        ("vector_format",po::value<string>()->default_value("b64"), "Sparse vector serialization format for feature expectations or gradients, includes (text or b64)")
        ("combine_size,C",po::value<int>()->default_value(1), "When option -G is used, process this many sentence pairs before writing the gradient (1=emit after every sentence pair)")
        ("forest_output,O",po::value<string>(),"Directory to write forests to")
        ("remove_intersected_rule_annotations", "After forced decoding is completed, remove nonterminal annotations (i.e., the source side spans)");

  // ob.AddOptions(&opts);
  po::options_description clo("Command line options");
  clo.add_options()
    ("config,c", po::value<vector<string> >(&cfg_files), "Configuration file(s) - latest has priority")
        ("help,?", "Print this help message and exit")
    ("usage,u", po::value<string>(), "Describe a feature function type")
    ("compgen", "Print just option names suitable for bash command line completion builtin 'compgen'")
    ;

  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts);

  dcmdline_options.add(dconfig_options).add(clo);
  if (argc) {
    po::store(parse_command_line(argc, argv, dcmdline_options), conf);
    if (conf.count("compgen")) {
      print_options(cout,dcmdline_options);
      cout << endl;
      exit(0);
    }
    if (conf.count("quiet"))
      SetSilent(true);
    if (!SILENT) ShowBanner();
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
  if (conf.count("quiet"))
    SetSilent(true);
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

  if (conf.count("list_feature_functions")) {
    cerr << "Available feature functions (specify with -F; describe with -u FeatureName):\n";
    ff_registry.DisplayList(); //TODO
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
  if (formalism != "scfg" && formalism != "fst" && formalism != "lextrans" && formalism != "pb" && formalism != "csplit" && formalism != "tagger" && formalism != "lexalign" && formalism != "rescore") {
    cerr << "Error: --formalism takes only 'scfg', 'fst', 'pb', 'csplit', 'lextrans', 'lexalign', 'rescore', or 'tagger'\n";
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
      (formalism != "csplit" || !(conf.count("beam_prune")||conf.count("density_prune")))) {
    cerr << "--csplit_preserve_full_word should only be "
         << "used with csplit AND --*_prune!\n";
    exit(1);
  }
  csplit_output_plf = conf.count("csplit_output_plf");
  if (csplit_output_plf && formalism != "csplit") {
    cerr << "--csplit_output_plf should only be used with csplit!\n";
    exit(1);
  }

  // load perfect hash function for features
  if (conf.count("cmph_perfect_feature_hash")) {
    cerr << "Loading perfect hash function from " << conf["cmph_perfect_feature_hash"].as<string>() << " ...\n";
    FD::EnableHash(conf["cmph_perfect_feature_hash"].as<string>());
    cerr << "  " << FD::NumFeats() << " features in map\n";
  }

  // load initial feature weights (and possibly freeze feature set)
  init_weights.reset(new vector<weight_t>);
  if (conf.count("weights"))
    Weights::InitFromFile(str("weights",conf), init_weights.get());

  if (conf.count("extract_rules")) {
    if (!DirectoryExists(conf["extract_rules"].as<string>()))
      MkDirP(conf["extract_rules"].as<string>());
  }

  // determine the number of rescoring/pruning/weighting passes configured
  const int MAX_PASSES = 3;
  for (int pass = 0; pass < MAX_PASSES; ++pass) {
    string ws = "weights" + StringSuffixForRescoringPass(pass);
    string ff = "feature_function" + StringSuffixForRescoringPass(pass);
    string sf = "summary_feature" + StringSuffixForRescoringPass(pass);
    string bp = "beam_prune" + StringSuffixForRescoringPass(pass);
    string dp = "density_prune" + StringSuffixForRescoringPass(pass);
    bool first_pass_condition = ((pass == 0) && (conf.count(ff) || conf.count(bp) || conf.count(dp)));
    bool nth_pass_condition = ((pass > 0) && (conf.count(ws) || conf.count(ff) || conf.count(bp) || conf.count(dp)));
    if (first_pass_condition || nth_pass_condition) {
      rescoring_passes.push_back(RescoringPass());
      RescoringPass& rp = rescoring_passes.back();
      // only configure new weights if pass > 0, otherwise we reuse the initial chart weights
      if (nth_pass_condition && conf.count(ws)) {
        rp.weight_vector.reset(new vector<weight_t>());
        Weights::InitFromFile(str(ws.c_str(), conf), rp.weight_vector.get());
      }
      bool has_stateful = false;
      if (conf.count(ff)) {
        vector<string> add_ffs;
        store_conf(conf,ff,&add_ffs);
        for (int i = 0; i < add_ffs.size(); ++i) {
          pffs.push_back(make_ff(add_ffs[i],verbose_feature_functions));
          FeatureFunction const* p=pffs.back().get();
          rp.ffs.push_back(p);
          if (p->IsStateful()) { has_stateful = true; }
        }
      }
      if (conf.count(sf)) {
        rp.fid_summary = FD::Convert(conf[sf].as<string>());
        assert(rp.fid_summary > 0);
        // TODO assert that weights for this pass have coef(fid_summary) == 0.0?
      }
      if (conf.count(bp)) { rp.beam_prune = conf[bp].as<double>(); }
      if (conf.count(dp)) { rp.density_prune = conf[dp].as<double>(); }
      int palg = (has_stateful ? 1 : 0);  // if there are no stateful featueres, default to FULL
      string isn = "intersection_strategy" + StringSuffixForRescoringPass(pass);
      string spl = "cubepruning_pop_limit" + StringSuffixForRescoringPass(pass);
      unsigned pop_limit = 200;
      if (conf.count(spl)) { pop_limit = conf[spl].as<unsigned>(); }
      if (LowercaseString(str(isn.c_str(),conf)) == "full") {
        palg = 0;
      }
      if (LowercaseString(conf["intersection_strategy"].as<string>()) == "fast_cube_pruning") {
        palg = 2;
        cerr << "Using Fast Cube Pruning intersection (see Algorithm 2 described in: Gesmundo A., Henderson J,. Faster Cube Pruning, IWSLT 2010).\n";
      }
      if (LowercaseString(conf["intersection_strategy"].as<string>()) == "fast_cube_pruning_2") {
        palg = 3;
        cerr << "Using Fast Cube Pruning 2 intersection (see Algorithm 3 described in: Gesmundo A., Henderson J,. Faster Cube Pruning, IWSLT 2010).\n";
      }
      rp.inter_conf.reset(new IntersectionConfiguration(palg, pop_limit));
    } else {
      break;  // TODO alert user if there are any future configurations
    }
  }

  // set up weight vectors since later phases may reuse weights from earlier phases
  boost::shared_ptr<vector<weight_t> > prev_weights = init_weights;
  for (int pass = 0; pass < rescoring_passes.size(); ++pass) {
    RescoringPass& rp = rescoring_passes[pass];
    if (!rp.weight_vector) {
      rp.weight_vector = prev_weights;
    } else {
      prev_weights = rp.weight_vector;
    }
    rp.models.reset(new ModelSet(*rp.weight_vector, rp.ffs));
  }

  // show configuration of rescoring passes
  if (!SILENT) {
    int num = rescoring_passes.size();
    cerr << "Configured " << num << " rescoring pass" << (num == 1 ? "" : "es") << endl;
    for (int pass = 0; pass < num; ++pass)
      cerr << "  " << rescoring_passes[pass] << endl;
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
  else if (formalism == "rescore")
    translator.reset(new RescoreTranslator(conf));
  else if (formalism == "tagger")
    translator.reset(new Tagger(conf));
  else
    assert(!"error");

  if (late_freeze) {
    cerr << "Late freezing feature set (use --no_freeze_feature_set to prevent)." << endl;
    FD::Freeze(); // this means we can't see the feature names of not-weighted features
  }

  sample_max_trans = conf.count("max_translation_sample") ?
    conf["max_translation_sample"].as<int>() : 0;
  if (sample_max_trans)
    rng.reset(new RandomNumberGenerator<boost::mt19937>);
  aligner_mode = conf.count("aligner");
  graphviz = conf.count("graphviz");
  joshua_viz = conf.count("show_joshua_visualization");
  encode_b64 = str("vector_format",conf) == "b64";
  kbest = conf.count("k_best");
  unique_kbest = conf.count("unique_k_best");
  get_oracle_forest = conf.count("get_oracle_forest");
  oracle.show_derivation=conf.count("show_derivations");
  remove_intersected_rule_annotations = conf.count("remove_intersected_rule_annotations");

  if (conf.count("extract_rules")) {
    stringstream ss;
    ss << sent_id;
    extract_file.reset(new WriteFile(str("extract_rules",conf)+"/"+ss.str()));
  }
  combine_size = conf["combine_size"].as<int>();
  if (combine_size < 1) combine_size = 1;
  sent_id = -1;
  acc_obj = 0; // accumulate objective
  g_count = 0;    // number of gradient pieces computed

  if (conf.count("incremental_search")) {
    incremental.reset(IncrementalBase::Load(conf["incremental_search"].as<string>().c_str(), CurrentWeightVector()));
  }
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
vector<weight_t>& Decoder::CurrentWeightVector() { return pimpl_->CurrentWeightVector(); }
const vector<weight_t>& Decoder::CurrentWeightVector() const { return pimpl_->CurrentWeightVector(); }
void Decoder::AddSupplementalGrammar(GrammarPtr gp) {
  static_cast<SCFGTranslator&>(*pimpl_->translator).AddSupplementalGrammar(gp);
}
void Decoder::AddSupplementalGrammarFromString(const std::string& grammar_string) {
  assert(pimpl_->translator->GetDecoderType() == "SCFG");
  static_cast<SCFGTranslator&>(*pimpl_->translator).AddSupplementalGrammarFromString(grammar_string);
}

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
    translator->Translate(to_translate, &smeta, *init_weights, &forest);
  translator->SentenceComplete();

  if (!translation_successful) {
    if (!SILENT) { cerr << "  NO PARSE FOUND.\n"; }
    o->NotifySourceParseFailure(smeta);
    o->NotifyDecodingComplete(smeta);
    if (conf.count("show_conditional_prob")) {
      cout << "-Inf" << endl << flush;
    } else if (!SILENT) {
      cout << endl;
    }
    return false;
  }

  const bool show_tree_structure=conf.count("show_tree_structure");
  if (!SILENT) forest_stats(forest,"  Init. forest",show_tree_structure,oracle.show_derivation);
  if (conf.count("show_expected_length")) {
    const PRPair<prob_t, prob_t> res =
      Inside<PRPair<prob_t, prob_t>,
             PRWeightFunction<prob_t, EdgeProb, prob_t, ELengthWeightFunction> >(forest);
    cerr << "  Expected length  (words): " << (res.r / res.p).as_float() << "\t" << res << endl;
  }

  if (conf.count("show_partition")) {
    const prob_t z = Inside<prob_t, EdgeProb>(forest);
    cerr << "  Partition         log(Z): " << log(z) << endl;
  }

  SummaryFeature summary_feature_type = kNODE_RISK;
  if (conf["summary_feature_type"].as<string>() == "edge_risk")
    summary_feature_type = kEDGE_RISK;
  else if (conf["summary_feature_type"].as<string>() == "node_risk")
    summary_feature_type = kNODE_RISK;
  else if (conf["summary_feature_type"].as<string>() == "edge_prob")
    summary_feature_type = kEDGE_PROB;
  else {
    cerr << "Bad summary_feature_type: " << conf["summary_feature_type"].as<string>() << endl;
    abort();
  }

  if (conf.count("show_target_graph")) {
    HypergraphIO::WriteTarget(conf["show_target_graph"].as<string>(), sent_id, forest);
  }
  if (conf.count("incremental_search")) {
    incremental->Search(conf["cubepruning_pop_limit"].as<unsigned>(), forest);
  }
  if (conf.count("show_target_graph") || conf.count("incremental_search")) {
    o->NotifyDecodingComplete(smeta);
    return true;
  }

  for (int pass = 0; pass < rescoring_passes.size(); ++pass) {
    const RescoringPass& rp = rescoring_passes[pass];
    const vector<weight_t>& cur_weights = *rp.weight_vector;
    if (!SILENT) cerr << endl << "  RESCORING PASS #" << (pass+1) << " " << rp << endl;

    string passtr = "Pass1"; passtr[4] += pass;
    forest.Reweight(cur_weights);
    const bool has_rescoring_models = !rp.models->empty();
    if (has_rescoring_models) {
      Timer t("Forest rescoring:");
      rp.models->PrepareForInput(smeta);
      Hypergraph rescored_forest;
#ifdef CP_TIME
      CpTime::Sub(clock());
#endif
      ApplyModelSet(forest,
                  smeta,
                  *rp.models,
                  *rp.inter_conf,
                  &rescored_forest);
#ifdef CP_TIME
      CpTime::Add(clock());
#endif
      forest.swap(rescored_forest);
      forest.Reweight(cur_weights);
      if (!SILENT) forest_stats(forest,"  " + passtr +" forest",show_tree_structure,oracle.show_derivation, conf.count("extract_rules"), extract_file);
    }

    if (conf.count("show_partition")) {
      const prob_t z = Inside<prob_t, EdgeProb>(forest);
      cerr << "  " << passtr << " partition     log(Z): " << log(z) << endl;
    }

    if (rp.fid_summary) {
      if (summary_feature_type == kEDGE_PROB) {
        const prob_t z = forest.PushWeightsToGoal(1.0);
        if (!std::isfinite(log(z)) || std::isnan(log(z))) {
          cerr << "  " << passtr << " !!! Invalid partition detected, abandoning.\n";
        } else {
          for (int i = 0; i < forest.edges_.size(); ++i) {
            const double log_prob_transition = log(forest.edges_[i].edge_prob_); // locally normalized by the edge
                                                                              // head node by forest.PushWeightsToGoal
            if (!std::isfinite(log_prob_transition) || std::isnan(log_prob_transition)) {
              cerr << "Edge: i=" << i << " got bad inside prob: " << *forest.edges_[i].rule_ << endl;
              abort();
            }

            forest.edges_[i].feature_values_.set_value(rp.fid_summary, log_prob_transition);
          }
          forest.Reweight(cur_weights);  // reset weights
        }
      } else if (summary_feature_type == kNODE_RISK) {
        Hypergraph::EdgeProbs posts;
        const prob_t z = forest.ComputeEdgePosteriors(1.0, &posts);
        if (!std::isfinite(log(z)) || std::isnan(log(z))) {
          cerr << "  " << passtr << " !!! Invalid partition detected, abandoning.\n";
        } else {
          for (int i = 0; i < forest.nodes_.size(); ++i) {
            const Hypergraph::EdgesVector& in_edges = forest.nodes_[i].in_edges_;
            prob_t node_post = prob_t(0);
            for (int j = 0; j < in_edges.size(); ++j)
              node_post += (posts[in_edges[j]] / z);
            const double log_np = log(node_post);
            if (!std::isfinite(log_np) || std::isnan(log_np)) {
              cerr << "got bad posterior prob for node " << i << endl;
              abort();
            }
            for (int j = 0; j < in_edges.size(); ++j)
              forest.edges_[in_edges[j]].feature_values_.set_value(rp.fid_summary, exp(log_np));
//            Hypergraph::Edge& example_edge = forest.edges_[in_edges[0]];
//            string n = "NONE";
//            if (forest.nodes_[i].cat_) n = TD::Convert(-forest.nodes_[i].cat_);
//            cerr << "[" << n << "," << example_edge.i_ << "," << example_edge.j_ << "] = " << exp(log_np) << endl;
          }
        }
      } else if (summary_feature_type == kEDGE_RISK) {
        Hypergraph::EdgeProbs posts;
        const prob_t z = forest.ComputeEdgePosteriors(1.0, &posts);
        if (!std::isfinite(log(z)) || std::isnan(log(z))) {
          cerr << "  " << passtr << " !!! Invalid partition detected, abandoning.\n";
        } else {
          assert(posts.size() == forest.edges_.size());
          for (int i = 0; i < posts.size(); ++i) {
            const double log_np = log(posts[i] / z);
            if (!std::isfinite(log_np) || std::isnan(log_np)) {
              cerr << "got bad posterior prob for node " << i << endl;
              abort();
            }
            forest.edges_[i].feature_values_.set_value(rp.fid_summary, exp(log_np));
          }
        }
      } else {
        assert(!"shouldn't happen");
      }
    }

    string fullbp = "beam_prune" + StringSuffixForRescoringPass(pass);
    string fulldp = "density_prune" + StringSuffixForRescoringPass(pass);
    maybe_prune(forest,conf,fullbp.c_str(),fulldp.c_str(),passtr,srclen);
  }

  const vector<double>& last_weights = (rescoring_passes.empty() ? *init_weights : *rescoring_passes.back().weight_vector);

  // Oracle Rescoring
  if(get_oracle_forest) {
    assert(!"this is broken"); SparseVector<double> dummy; // = last_weights
    Oracle oc=oracle.ComputeOracle(smeta,&forest,dummy,10,conf["forest_output"].as<std::string>());
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
        if (!succeeded) abort();
      }
      HG::Union(forest, &new_hg);
      bool succeeded = writer.Write(new_hg, false);
      if (!succeeded) abort();
    } else {
      bool succeeded = writer.Write(forest, false);
      if (!succeeded) abort();
    }
  }

  // TODO I think this should probably be handled by an Observer
  if (sample_max_trans) {
    MaxTranslationSample(&forest, sample_max_trans, conf.count("k_best") ? conf["k_best"].as<int>() : 0);
  } else {
    if (kbest && !has_ref) {
      //TODO: does this work properly?
      const string deriv_fname = conf.count("show_derivations") ? str("show_derivations",conf) : "-";
      oracle.DumpKBest(sent_id, forest, conf["k_best"].as<int>(), unique_kbest,"-", deriv_fname);
    } else if (csplit_output_plf) {
      cout << HypergraphIO::AsPLF(forest, false) << endl;
    } else {
      if (!graphviz && !has_ref && !joshua_viz && !SILENT) {
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
//      if (crf_uniform_empirical) {
//        if (!SILENT) cerr << "  USING UNIFORM WEIGHTS\n";
//        for (int i = 0; i < forest.edges_.size(); ++i)
//          forest.edges_[i].edge_prob_=prob_t::One(); }
      if (remove_intersected_rule_annotations) {
        for (unsigned i = 0; i < forest.edges_.size(); ++i)
          if (forest.edges_[i].rule_ &&
              forest.edges_[i].rule_->parent_rule_)
            forest.edges_[i].rule_ = forest.edges_[i].rule_->parent_rule_;
      }
      forest.Reweight(last_weights);
      if (!SILENT) forest_stats(forest,"  Constr. forest",show_tree_structure,oracle.show_derivation);
      if (!SILENT) cerr << "  Constr. VitTree: " << ViterbiFTree(forest) << endl;
      if (conf.count("show_partition")) {
         const prob_t z = Inside<prob_t, EdgeProb>(forest);
         cerr << "  Contst. partition  log(Z): " << log(z) << endl;
      }
      o->NotifyAlignmentForest(smeta, &forest);
      if (conf.count("show_cfg_alignment_space"))
        HypergraphIO::WriteAsCFG(forest);
      if (conf.count("forest_output")) {
        ForestWriter writer(str("forest_output",conf), sent_id);
        if (FileExists(writer.fname_)) {
          if (!SILENT) cerr << "  Unioning...\n";
          Hypergraph new_hg;
          {
            ReadFile rf(writer.fname_);
            bool succeeded = HypergraphIO::ReadFromJSON(rf.stream(), &new_hg);
            if (!succeeded) abort();
          }
          HG::Union(forest, &new_hg);
          bool succeeded = writer.Write(new_hg, false);
          if (!succeeded) abort();
        } else {
          bool succeeded = writer.Write(forest, false);
          if (!succeeded) abort();
        }
      }
      if (aligner_mode && !output_training_vector)
        AlignerTools::WriteAlignment(smeta.GetSourceLattice(), smeta.GetReference(), forest, &cout, 0 == conf.count("aligner_use_viterbi"), kbest ? conf["k_best"].as<int>() : 0);
      if (write_gradient) {
        const prob_t ref_z = InsideOutside<prob_t, EdgeProb, SparseVector<prob_t>, EdgeFeaturesAndProbWeightFunction>(forest, &ref_exp);
        ref_exp /= ref_z;
//        if (crf_uniform_empirical)
//          log_ref_z = ref_exp.dot(last_weights);
        log_ref_z = log(ref_z);
        //cerr << "      MODEL LOG Z: " << log_z << endl;
        //cerr << "  EMPIRICAL LOG Z: " << log_ref_z << endl;
        if ((log_z - log_ref_z) < kMINUS_EPSILON) {
          cerr << "DIFF. ERR! log_z < log_ref_z: " << log_z << " " << log_ref_z << endl;
          exit(1);
        }
        assert(!std::isnan(log_ref_z));
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
      if (kbest) {
        const string deriv_fname = conf.count("show_derivations") ? str("show_derivations",conf) : "-";
        oracle.DumpKBest(sent_id, forest, conf["k_best"].as<int>(), unique_kbest,"-", deriv_fname);
      }
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

