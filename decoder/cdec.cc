#include <iostream>
#include <fstream>
#include <tr1/unordered_map>
#include <tr1/unordered_set>

#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "timing_stats.h"
#include "translator.h"
#include "phrasebased_translator.h"
#include "aligner.h"
#include "stringlib.h"
#include "forest_writer.h"
#include "hg_io.h"
#include "filelib.h"
#include "sampler.h"
#include "sparse_vector.h"
#include "tagger.h"
#include "lextrans.h"
#include "lexalign.h"
#include "csplit.h"
#include "weights.h"
#include "tdict.h"
#include "ff.h"
#include "ff_factory.h"
#include "hg_intersect.h"
#include "apply_models.h"
#include "viterbi.h"
#include "kbest.h"
#include "inside_outside.h"
#include "exp_semiring.h"
#include "sentence_metadata.h"

using namespace std;
using namespace std::tr1;
using boost::shared_ptr;
namespace po = boost::program_options;

// some globals ...
boost::shared_ptr<RandomNumberGenerator<boost::mt19937> > rng;
static const double kMINUS_EPSILON = -1e-6;  // don't be too strict

namespace Hack { void MaxTrans(const Hypergraph& in, int beam_size); }
namespace NgramCache { void Clear(); }

void ShowBanner() {
  cerr << "cdec v1.0 (c) 2009-2010 by Chris Dyer\n";
}

void ConvertSV(const SparseVector<prob_t>& src, SparseVector<double>* trg) {
  for (SparseVector<prob_t>::const_iterator it = src.begin(); it != src.end(); ++it)
    trg->set_value(it->first, it->second);
}

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("formalism,f",po::value<string>(),"Decoding formalism; values include SCFG, FST, PB, LexTrans (lexical translation model, also disc training), CSplit (compound splitting), Tagger (sequence labeling), LexAlign (alignment only, or EM training)")
        ("input,i",po::value<string>()->default_value("-"),"Source file")
        ("grammar,g",po::value<vector<string> >()->composing(),"Either SCFG grammar file(s) or phrase tables file(s)")
        ("weights,w",po::value<string>(),"Feature weights file")
        ("no_freeze_feature_set,Z", "Do not freeze feature set after reading feature weights file")
        ("feature_function,F",po::value<vector<string> >()->composing(), "Additional feature function(s) (-L for list)")
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
	("show_tree_structure", "Show the Viterbi derivation structure")
        ("show_expected_length", "Show the expected translation length under the model")
        ("show_partition,z", "Compute and show the partition (inside score)")
        ("show_cfg_search_space", "Show the search space as a CFG")
        ("beam_prune", po::value<double>(), "Prune paths from +LM forest")
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
        ("feature_expectations","Write feature expectations for all features in chart (**OBJ** will be the partition)")
        ("vector_format",po::value<string>()->default_value("b64"), "Sparse vector serialization format for feature expectations or gradients, includes (text or b64)")
        ("combine_size,C",po::value<int>()->default_value(1), "When option -G is used, process this many sentence pairs before writing the gradient (1=emit after every sentence pair)")
        ("forest_output,O",po::value<string>(),"Directory to write forests to")
        ("minimal_forests,m","Write minimal forests (excludes Rule information). Such forests can be used for ML/MAP training, but not rescoring, etc.");
  po::options_description clo("Command line options");
  clo.add_options()
        ("config,c", po::value<string>(), "Configuration file")
        ("help,h", "Print this help message and exit");
  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts);
  dcmdline_options.add(opts).add(clo);

  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("config")) {
    const string cfg = (*conf)["config"].as<string>();
    cerr << "Configuration file: " << cfg << endl;
    ifstream config(cfg.c_str());
    po::store(po::parse_config_file(config, dconfig_options), *conf);
  }
  po::notify(*conf);

  if (conf->count("list_feature_functions")) {
    cerr << "Available feature functions (specify with -F):\n";
    global_ff_registry->DisplayList();
    cerr << endl;
    exit(1);
  }

  if (conf->count("help") || conf->count("formalism") == 0) {
    cerr << dcmdline_options << endl;
    exit(1);
  }

  const string formalism = LowercaseString((*conf)["formalism"].as<string>());
  if (formalism != "scfg" && formalism != "fst" && formalism != "lextrans" && formalism != "pb" && formalism != "csplit" && formalism != "tagger" && formalism != "lexalign") {
    cerr << "Error: --formalism takes only 'scfg', 'fst', 'pb', 'csplit', 'lextrans', 'lexalign', or 'tagger'\n";
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

// TODO move out of cdec into some sampling decoder file
void SampleRecurse(const Hypergraph& hg, const vector<SampleSet>& ss, int n, vector<WordID>* out) {
  const SampleSet& s = ss[n];
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

// TODO move out of cdec into some sampling decoder file
void MaxTranslationSample(Hypergraph* hg, const int samples, const int k) {
  unordered_map<string, int, boost::hash<string> > m;
  hg->PushWeightsToGoal();
  const int num_nodes = hg->nodes_.size();
  vector<SampleSet> ss(num_nodes);
  for (int i = 0; i < num_nodes; ++i) {
    SampleSet& s = ss[i];
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

// TODO decoder output should probably be moved to another file
void DumpKBest(const int sent_id, const Hypergraph& forest, const int k, const bool unique) {
cerr << "In kbest\n"; 
 if (unique) {
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal, KBest::FilterUnique> kbest(forest, k);
    for (int i = 0; i < k; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal, KBest::FilterUnique>::Derivation* d =
        kbest.LazyKthBest(forest.nodes_.size() - 1, i);
      if (!d) break;
      cout << sent_id << " ||| " << TD::GetString(d->yield) << " ||| "
           << d->feature_values << " ||| " << log(d->score) << endl;
    }
  } else {
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(forest, k);
    for (int i = 0; i < k; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
        kbest.LazyKthBest(forest.nodes_.size() - 1, i);
      if (!d) break;
      cout << sent_id << " ||| " << TD::GetString(d->yield) << " ||| "
           << d->feature_values << " ||| " << log(d->score) << endl;
    }
  }
}

struct ELengthWeightFunction {
  double operator()(const Hypergraph::Edge& e) const {
    return e.rule_->ELength() - e.rule_->Arity();
  }
};


struct TRPHash {
  size_t operator()(const TRulePtr& o) const { return reinterpret_cast<size_t>(o.get()); }
};
static void ExtractRulesDedupe(const Hypergraph& hg, ostream* os) {
  static unordered_set<TRulePtr, TRPHash> written;
  for (int i = 0; i < hg.edges_.size(); ++i) {
    const TRulePtr& rule = hg.edges_[i].rule_;
    if (written.insert(rule).second) {
      (*os) << rule->AsString() << endl;
    }
  }
}

void register_feature_functions();

int main(int argc, char** argv) {
  global_ff_registry.reset(new FFRegistry);
  register_feature_functions();
  ShowBanner();
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const bool write_gradient = conf.count("cll_gradient");
  const bool feature_expectations = conf.count("feature_expectations");
  if (write_gradient && feature_expectations) {
    cerr << "You can only specify --gradient or --feature_expectations, not both!\n";
    exit(1);
  }
  const bool output_training_vector = (write_gradient || feature_expectations);

  boost::shared_ptr<Translator> translator;
  const string formalism = LowercaseString(conf["formalism"].as<string>());
  const bool csplit_preserve_full_word = conf.count("csplit_preserve_full_word");
  if (csplit_preserve_full_word &&
      (formalism != "csplit" || !conf.count("beam_prune"))) {
    cerr << "--csplit_preserve_full_word should only be "
         << "used with csplit AND --beam_prune!\n";
    exit(1);
  }
  const bool csplit_output_plf = conf.count("csplit_output_plf");
  if (csplit_output_plf && formalism != "csplit") {
    cerr << "--csplit_output_plf should only be used with csplit!\n";
    exit(1);
  }

  // load feature weights (and possibly freeze feature set)
  vector<double> feature_weights;
  Weights w;
  if (conf.count("weights")) {
    w.InitFromFile(conf["weights"].as<string>());
    feature_weights.resize(FD::NumFeats());
    w.InitVector(&feature_weights);
    if (!conf.count("no_freeze_feature_set")) {
      cerr << "Freezing feature set (use --no_freeze_feature_set to change)." << endl;
      FD::Freeze();
    }
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
  vector<shared_ptr<FeatureFunction> > pffs;
  vector<const FeatureFunction*> late_ffs;
  if (conf.count("feature_function") > 0) {
    const vector<string>& add_ffs = conf["feature_function"].as<vector<string> >();
    for (int i = 0; i < add_ffs.size(); ++i) {
      string ff, param;
      SplitCommandAndParam(add_ffs[i], &ff, &param);
      cerr << "Feature: " << ff;
      if (param.size() > 0) cerr << " (with config parameters '" << param << "')\n";
      else cerr << " (no config parameters)\n";
      shared_ptr<FeatureFunction> pff = global_ff_registry->Create(ff, param);
      if (!pff) { exit(1); }
      // TODO check that multiple features aren't trying to set the same fid
      pffs.push_back(pff);
      late_ffs.push_back(pff.get());
    }
  }
  ModelSet late_models(feature_weights, late_ffs);
  int palg = 1;
  if (LowercaseString(conf["intersection_strategy"].as<string>()) == "full") {
    palg = 0;
    cerr << "Using full intersection (no pruning).\n";
  }
  const IntersectionConfiguration inter_conf(palg, conf["cubepruning_pop_limit"].as<int>());

  const int sample_max_trans = conf.count("max_translation_sample") ?
    conf["max_translation_sample"].as<int>() : 0;
  if (sample_max_trans)
    rng.reset(new RandomNumberGenerator<boost::mt19937>);
  const bool aligner_mode = conf.count("aligner");
  const bool minimal_forests = conf.count("minimal_forests");
  const bool graphviz = conf.count("graphviz");
  const bool encode_b64 = conf["vector_format"].as<string>() == "b64";
  const bool kbest = conf.count("k_best");
  const bool unique_kbest = conf.count("unique_k_best");
  const bool crf_uniform_empirical = conf.count("crf_uniform_empirical");
  shared_ptr<WriteFile> extract_file;
  if (conf.count("extract_rules"))
    extract_file.reset(new WriteFile(conf["extract_rules"].as<string>()));

  int combine_size = conf["combine_size"].as<int>();
  if (combine_size < 1) combine_size = 1;
  const string input = conf["input"].as<string>();
  cerr << "Reading input from " << ((input == "-") ? "STDIN" : input.c_str()) << endl;
  ReadFile in_read(input);
  istream *in = in_read.stream();
  assert(*in);

  SparseVector<prob_t> acc_vec;  // accumulate gradient
  double acc_obj = 0; // accumulate objective
  int g_count = 0;    // number of gradient pieces computed
  int sent_id = -1;         // line counter

  while(*in) {
    NgramCache::Clear();   // clear ngram cache for remote LM (if used)
    Timer::Summarize();
    ++sent_id;
    string buf;
    getline(*in, buf);
    if (buf.empty()) continue;
    map<string, string> sgml;
    ProcessAndStripSGML(&buf, &sgml);
    if (sgml.find("id") != sgml.end())
      sent_id = atoi(sgml["id"].c_str());

    cerr << "\nINPUT: ";
    if (buf.size() < 100)
      cerr << buf << endl;
    else {
     size_t x = buf.rfind(" ", 100);
     if (x == string::npos) x = 100;
     cerr << buf.substr(0, x) << " ..." << endl;
    }
    cerr << "  id = " << sent_id << endl;
    string to_translate;
    Lattice ref;
    ParseTranslatorInputLattice(buf, &to_translate, &ref);
    const bool has_ref = ref.size() > 0;
    SentenceMetadata smeta(sent_id, ref);
    const bool hadoop_counters = (write_gradient);
    Hypergraph forest;          // -LM forest
    translator->ProcessMarkupHints(sgml);
    Timer t("Translation");
    const bool translation_successful =
      translator->Translate(to_translate, &smeta, feature_weights, &forest);
    translator->SentenceComplete();
    if (!translation_successful) {
      cerr << "  NO PARSE FOUND.\n";
      if (hadoop_counters)
        cerr << "reporter:counter:UserCounters,FParseFailed,1" << endl;
      cout << endl << flush;
      continue;
    }
    cerr << "  -LM forest (nodes/edges): " << forest.nodes_.size() << '/' << forest.edges_.size() << endl;
    cerr << "  -LM forest       (paths): " << forest.NumberOfPaths() << endl;
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
    if (extract_file)
      ExtractRulesDedupe(forest, extract_file->stream());
    vector<WordID> trans;
    const prob_t vs = ViterbiESentence(forest, &trans);
    cerr << "  -LM Viterbi: " << TD::GetString(trans) << endl;
    if (conf.count("show_tree_structure"))
      cerr << "  -LM    tree: " << ViterbiETree(forest) << endl;;
    cerr << "  -LM Viterbi: " << log(vs) << endl;

    bool has_late_models = !late_models.empty();
    if (has_late_models) {
      forest.Reweight(feature_weights);
      forest.SortInEdgesByEdgeWeights();
      Hypergraph lm_forest;
      ApplyModelSet(forest,
                    smeta,
                    late_models,
                    inter_conf,
                    &lm_forest);
      forest.swap(lm_forest);
      forest.Reweight(feature_weights);
      trans.clear();
      ViterbiESentence(forest, &trans);
      cerr << "  +LM forest (nodes/edges): " << forest.nodes_.size() << '/' << forest.edges_.size() << endl;
      cerr << "  +LM forest       (paths): " << forest.NumberOfPaths() << endl;
      cerr << "  +LM Viterbi: " << TD::GetString(trans) << endl;
    }
    if (conf.count("beam_prune")) {
      vector<bool> preserve_mask(forest.edges_.size(), false);
      if (csplit_preserve_full_word)
        preserve_mask[CompoundSplit::GetFullWordEdgeIndex(forest)] = true;
      forest.BeamPruneInsideOutside(1.0, false, conf["beam_prune"].as<double>(), &preserve_mask);
      cerr << "  Pruned forest    (paths): " << forest.NumberOfPaths() << endl;
    }

    if (conf.count("forest_output") && !has_ref) {
      ForestWriter writer(conf["forest_output"].as<string>(), sent_id);
      if (FileExists(writer.fname_)) {
        cerr << "  Unioning...\n";
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

    if (sample_max_trans) {
      MaxTranslationSample(&forest, sample_max_trans, conf.count("k_best") ? conf["k_best"].as<int>() : 0);
    } else {
      if (kbest) {
        DumpKBest(sent_id, forest, conf["k_best"].as<int>(), unique_kbest);
      } else if (csplit_output_plf) {
        cout << HypergraphIO::AsPLF(forest, false) << endl;
      } else {
        if (!graphviz && !has_ref) {
          cout << TD::GetString(trans) << endl << flush;
        }
      }
    }

    const int max_trans_beam_size = conf.count("max_translation_beam") ?
      conf["max_translation_beam"].as<int>() : 0;
    if (max_trans_beam_size) {
      Hack::MaxTrans(forest, max_trans_beam_size);
      continue;
    }

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
        cerr << "  Constr. forest (nodes/edges): " << forest.nodes_.size() << '/' << forest.edges_.size() << endl;
        cerr << "  Constr. forest       (paths): " << forest.NumberOfPaths() << endl;
        if (crf_uniform_empirical) {
          cerr << "  USING UNIFORM WEIGHTS\n";
          for (int i = 0; i < forest.edges_.size(); ++i)
            forest.edges_[i].edge_prob_=prob_t::One();
        } else {
          forest.Reweight(feature_weights);
          cerr << "  Constr. VitTree: " << ViterbiFTree(forest) << endl;
        }
	if (hadoop_counters)
          cerr << "reporter:counter:UserCounters,SentencePairsParsed,1" << endl;
        if (conf.count("show_partition")) {
           const prob_t z = Inside<prob_t, EdgeProb>(forest);
           cerr << "  Contst. partition  log(Z): " << log(z) << endl;
        }
        //DumpKBest(sent_id, forest, 1000);
        if (conf.count("forest_output")) {
          ForestWriter writer(conf["forest_output"].as<string>(), sent_id);
          if (FileExists(writer.fname_)) {
            cerr << "  Unioning...\n";
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
          AlignerTools::WriteAlignment(smeta.GetSourceLattice(), smeta.GetReference(), forest, &cout);
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
          acc_vec.clear_value(0);
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
      } else {
        cerr << "  REFERENCE UNREACHABLE.\n";
        if (write_gradient) {
	  if (hadoop_counters)
            cerr << "reporter:counter:UserCounters,EFParseFailed,1" << endl;
          cout << endl << flush;
	}
      }
    }
  }
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

