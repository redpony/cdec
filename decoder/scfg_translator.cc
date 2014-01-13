#include <algorithm>
#include <vector>
#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>
#include "hash.h"
#include "translator.h"
#include "hg.h"
#include "grammar.h"
#include "bottom_up_parser.h"
#include "sentence_metadata.h"
#include "stringlib.h"
#include "tdict.h"
#include "viterbi.h"
#include "verbose.h"

#define foreach         BOOST_FOREACH
#define reverse_foreach BOOST_REVERSE_FOREACH

using namespace std;
static bool printGrammarsUsed = false;

struct GlueGrammar : public TextGrammar {
  // read glue grammar from file
  explicit GlueGrammar(const std::string& file);
  GlueGrammar(const std::string& goal_nt, const std::string& default_nt, const unsigned int ctf_level=0);  // "S", "X"
  virtual bool HasRuleForSpan(int i, int j, int distance) const;
};

struct PassThroughGrammar : public TextGrammar {
  PassThroughGrammar(const Lattice& input, const std::string& cat, const unsigned int ctf_level=0);
  virtual bool HasRuleForSpan(int i, int j, int distance) const;
};

GlueGrammar::GlueGrammar(const string& file) : TextGrammar(file) {}

static void RefineRule(TRulePtr pt, const unsigned int ctf_level){
  for (unsigned int i=0; i<ctf_level; ++i){
    TRulePtr r(new TRule(*pt));
    pt->fine_rules_.reset(new vector<TRulePtr>);
    pt->fine_rules_->push_back(r);
    pt = r;
  }
}

GlueGrammar::GlueGrammar(const string& goal_nt, const string& default_nt, const unsigned int ctf_level) {
  TRulePtr stop_glue(new TRule("[" + goal_nt + "] ||| [" + default_nt + ",1] ||| [1]"));
  AddRule(stop_glue);
  RefineRule(stop_glue, ctf_level);
  TRulePtr glue(new TRule("[" + goal_nt + "] ||| [" + goal_nt + ",1] ["+ default_nt + ",2] ||| [1] [2] ||| Glue=1"));
  AddRule(glue);
  RefineRule(glue, ctf_level);
}

bool GlueGrammar::HasRuleForSpan(int i, int /* j */, int /* distance */) const {
  return (i == 0);
}

PassThroughGrammar::PassThroughGrammar(const Lattice& input, const string& cat, const unsigned int ctf_level) {
  unordered_set<WordID> ss;
  for (int i = 0; i < input.size(); ++i) {
    const vector<LatticeArc>& alts = input[i];
    for (int k = 0; k < alts.size(); ++k) {
      const int j = alts[k].dist2next + i;
      const string& src = TD::Convert(alts[k].label);
      if (ss.count(alts[k].label) == 0) {
        int length = static_cast<int>(log(UTF8StringLen(src)) / log(1.6)) + 1;
        if (length > 6) length = 6;
        string len_feat = "PassThrough_0=1";
        len_feat[12] += length;
        TRulePtr pt(new TRule("[" + cat + "] ||| " + src + " ||| " + src + " ||| PassThrough=1 " + len_feat));
        pt->a_.push_back(AlignmentPoint(0,0));
        AddRule(pt);
        RefineRule(pt, ctf_level);
        ss.insert(alts[k].label);
      }
    }
  }
}

bool PassThroughGrammar::HasRuleForSpan(int, int, int distance) const {
  return (distance < 4);  // TODO this isn't great, but helps with EPS lattices
}

struct SCFGTranslatorImpl {
  SCFGTranslatorImpl(const boost::program_options::variables_map& conf) :
      max_span_limit(conf["scfg_max_span_limit"].as<int>()),
      add_pass_through_rules(conf.count("add_pass_through_rules")),
      goal(conf["goal"].as<string>()),
      default_nt(conf["scfg_default_nt"].as<string>()),
      use_ctf_(conf.count("coarse_to_fine_beam_prune"))
  {
    if(conf.count("grammar")){
      vector<string> gfiles = conf["grammar"].as<vector<string> >();
      for (unsigned i = 0; i < gfiles.size(); ++i) {
        if (!SILENT) cerr << "Reading SCFG grammar from " << gfiles[i] << endl;
        TextGrammar* g = new TextGrammar(gfiles[i]);
        g->SetMaxSpan(max_span_limit);
        g->SetGrammarName(gfiles[i]);
        grammars.push_back(GrammarPtr(g));
      }
      if (!SILENT) cerr << endl;
    }
    if (conf.count("scfg_extra_glue_grammar")) {
      GlueGrammar* g = new GlueGrammar(conf["scfg_extra_glue_grammar"].as<string>());
      g->SetGrammarName("ExtraGlueGrammar");
      grammars.push_back(GrammarPtr(g));
      if (!SILENT) cerr << "Adding glue grammar from file " << conf["scfg_extra_glue_grammar"].as<string>() << endl;
    }
    ctf_iterations_=0;
    if (use_ctf_){
      ctf_alpha_ = conf["coarse_to_fine_beam_prune"].as<double>();
      foreach(GrammarPtr& gp, grammars){
        ctf_iterations_ = std::max(gp->GetCTFLevels(), ctf_iterations_);
      }
      foreach(GrammarPtr& gp, grammars){
        if (gp->GetCTFLevels() != ctf_iterations_){
          cerr << "Grammar " << gp->GetGrammarName() << " has CTF level " << gp->GetCTFLevels() <<
            " but overall number of CTF levels is " << ctf_iterations_ << endl <<
            "Mixing coarse-to-fine grammars of different granularities is not supported" << endl;
          abort();
        }
      }
      show_tree_structure_ = conf.count("show_tree_structure");
      ctf_wide_alpha_ = conf["ctf_beam_widen"].as<double>();
      ctf_num_widenings_ = conf["ctf_num_widenings"].as<int>();
      ctf_exhaustive_ = (conf.count("ctf_no_exhaustive") == 0);
      assert(ctf_wide_alpha_ > 1.0);
      cerr << "Using coarse-to-fine pruning with " << ctf_iterations_ << " grammar projection(s) and alpha=" << ctf_alpha_ << endl;
      cerr << "  Coarse beam will be widened " << ctf_num_widenings_ << " times by a factor of " << ctf_wide_alpha_ << " if fine parse fails" << endl;
    }
    if (!conf.count("scfg_no_hiero_glue_grammar")){
      GlueGrammar* g = new GlueGrammar(goal, default_nt, ctf_iterations_);
      g->SetGrammarName("GlueGrammar");
      grammars.push_back(GrammarPtr(g));
      if (!SILENT) cerr << "Adding glue grammar for default nonterminal " << default_nt <<
        " and goal nonterminal " << goal << endl;
    }
 }

  const int max_span_limit;
  const bool add_pass_through_rules;
  const string goal;
  const string default_nt;
  const bool use_ctf_;
  double ctf_alpha_;
  double ctf_wide_alpha_;
  int ctf_num_widenings_;
  bool ctf_exhaustive_;
  bool show_tree_structure_;
  unsigned int ctf_iterations_;
  vector<GrammarPtr> grammars;
  set<GrammarPtr> sup_grammars_;

  struct ContainedIn {
    ContainedIn(const set<GrammarPtr>& gs) : gs_(gs) {}
    bool operator()(const GrammarPtr& x) const { return gs_.find(x) != gs_.end(); }
    const set<GrammarPtr>& gs_;
  };

  void AddSupplementalGrammarFromString(const std::string& grammar_string) {
    grammars.erase(remove_if(grammars.begin(), grammars.end(), ContainedIn(sup_grammars_)), grammars.end());
    istringstream in(grammar_string);
    TextGrammar* sent_grammar = new TextGrammar(&in);
    sent_grammar->SetMaxSpan(max_span_limit);
    sent_grammar->SetGrammarName("SupFromString");
    AddSupplementalGrammar(GrammarPtr(sent_grammar));
  }

  void AddSupplementalGrammar(GrammarPtr gp) {
    sup_grammars_.insert(gp);
    grammars.push_back(gp);
  }

  void RemoveSupplementalGrammars() {
    grammars.erase(remove_if(grammars.begin(), grammars.end(), ContainedIn(sup_grammars_)), grammars.end());
    sup_grammars_.clear();
  }

  bool Translate(const string& input,
                 SentenceMetadata* smeta,
                 const vector<double>& weights,
                 Hypergraph* forest) {
    vector<GrammarPtr> glist = grammars;
    Lattice& lattice = smeta->src_lattice_;
    LatticeTools::ConvertTextOrPLF(input, &lattice);
    smeta->SetSourceLength(lattice.size());
    if (add_pass_through_rules){
      if (!SILENT) cerr << "Adding pass through grammar" << endl;
      PassThroughGrammar* g = new PassThroughGrammar(lattice, default_nt, ctf_iterations_);
      g->SetGrammarName("PassThrough");
      glist.push_back(GrammarPtr(g));
    }
    for (unsigned gi = 0; gi < glist.size(); ++gi) {
      if(printGrammarsUsed)
        cerr << "Using grammar::" << glist[gi]->GetGrammarName() << endl;
    }
    if (!SILENT) cerr << "First pass parse... " << endl;
    ExhaustiveBottomUpParser parser(goal, glist);
    if (!parser.Parse(lattice, forest)){
      if (!SILENT) cerr << "  parse failed." << endl;
      return false;
    } else {
      // if (!SILENT) cerr << "  parse succeeded." << endl;
    }
    forest->Reweight(weights);
    if (use_ctf_) {
      Hypergraph::Node& goal_node = *(forest->nodes_.end()-1);
      foreach(unsigned edge_id, goal_node.in_edges_)
        RefineRule(forest->edges_[edge_id].rule_, ctf_iterations_);
      double alpha = ctf_alpha_;
      bool found_parse=false;
      for (int i=-1; i < ctf_num_widenings_; ++i) {
        cerr << "Coarse-to-fine source parse, alpha=" << alpha << endl;
        found_parse = true;
        Hypergraph refined_forest = *forest;
        for (unsigned j=0; j < ctf_iterations_; ++j) {
          cerr << viterbi_stats(refined_forest,"  Coarse forest",true,show_tree_structure_);
          cerr << "  Iteration " << (j+1) << ": Pruning forest... ";
          refined_forest.BeamPruneInsideOutside(1.0, false, alpha, NULL);
          cerr << "Refining forest...";
          if (RefineForest(&refined_forest)) {
            cerr << "  Refinement succeeded." << endl;
            refined_forest.Reweight(weights);
          } else {
            cerr << "  Refinement failed. Widening beam." << endl;
            found_parse = false;
            break;
          }
        }
        if (found_parse) {
          forest->swap(refined_forest);
          break;
        }
        alpha *= ctf_wide_alpha_;
      }
      if (!found_parse){
        if (ctf_exhaustive_){
          cerr << "Last resort: refining coarse forest without pruning...";
          for (unsigned j=0; j < ctf_iterations_; ++j) {
            if (RefineForest(forest)){
              cerr << "  Refinement succeeded." << endl;
              forest->Reweight(weights);
            } else {
              cerr << "  Refinement failed.  No parse found for this sentence." << endl;
              return false;
            }
          }
        } else
          return false;
      }
    }
    return true;
  }

  typedef std::pair<int, WordID> StateSplit;
  typedef std::pair<StateSplit, int> StateSplitPair;
  typedef HASH_MAP<StateSplit, int, boost::hash<StateSplit> > Split2Node;
  typedef HASH_MAP<int, vector<WordID> > Splits;

  bool RefineForest(Hypergraph* forest) {
    Hypergraph refined_forest;
    Split2Node s2n;
    HASH_MAP_RESERVED(s2n,StateSplit(-1,-1),StateSplit(-2,-2));
    Splits splits;
    HASH_MAP_RESERVED(splits,-1,-2);
    Hypergraph::Node& coarse_goal_node = *(forest->nodes_.end()-1);
    bool refined_goal_node = false;
    foreach(Hypergraph::Node& node, forest->nodes_){
      cerr << ".";
      foreach(int edge_id, node.in_edges_) {
        Hypergraph::Edge& edge = forest->edges_[edge_id];
        std::vector<int> nt_positions;
        TRulePtr& coarse_rule_ptr = edge.rule_;
        for(unsigned i=0; i< coarse_rule_ptr->f_.size(); ++i){
          if (coarse_rule_ptr->f_[i] < 0)
            nt_positions.push_back(i);
        }
        if (coarse_rule_ptr->fine_rules_ == 0) {
          cerr << "Parsing with mixed levels of coarse-to-fine granularity is currently unsupported." <<
            endl << "Could not find refinement for: " << coarse_rule_ptr->AsString() << " on edge " << edge_id << " spanning " << edge.i_ << "," << edge.j_ << endl;
          abort();
        }
        // fine rules apply only if state splits on tail nodes match fine rule nonterminals
        foreach(TRulePtr& fine_rule_ptr, *(coarse_rule_ptr->fine_rules_)) {
          Hypergraph::TailNodeVector tail;
          for (unsigned pos_i=0; pos_i<nt_positions.size(); ++pos_i){
            WordID fine_cat = fine_rule_ptr->f_[nt_positions[pos_i]];
            Split2Node::iterator it =
              s2n.find(StateSplit(edge.tail_nodes_[pos_i], fine_cat));
            if (it == s2n.end())
              break;
            else
              tail.push_back(it->second);
          }
          if (tail.size() == nt_positions.size()) {
            WordID cat = fine_rule_ptr->lhs_;
            Hypergraph::Edge* new_edge = refined_forest.AddEdge(fine_rule_ptr, tail);
            new_edge->i_ = edge.i_;
            new_edge->j_ = edge.j_;
            new_edge->feature_values_ = fine_rule_ptr->GetFeatureValues();
            new_edge->feature_values_.set_value(FD::Convert("LatticeCost"),
              edge.feature_values_.value(FD::Convert("LatticeCost")));
            Hypergraph::Node* head_node;
            Split2Node::iterator it = s2n.find(StateSplit(node.id_, cat));
            if (it == s2n.end()){
              head_node = refined_forest.AddNode(cat);
              s2n.insert(StateSplitPair(StateSplit(node.id_, cat), head_node->id_));
              splits[node.id_].push_back(cat);
              if (&node == &coarse_goal_node)
                refined_goal_node = true;
            } else
              head_node = &(refined_forest.nodes_[it->second]);
            refined_forest.ConnectEdgeToHeadNode(new_edge, head_node);
          }
        }
      }
    }
    cerr << endl;
    forest->swap(refined_forest);
    if (!refined_goal_node)
      return false;
    return true;
  }
  void OutputForest(Hypergraph* h) {
    foreach(Hypergraph::Node& n, h->nodes_){
      if (n.in_edges_.size() == 0){
        cerr << "<" << TD::Convert(-n.cat_) << ", ?, ?>" << endl;
      } else {
        cerr << "<" << TD::Convert(-n.cat_) << ", " << h->edges_[n.in_edges_[0]].i_ << ", " << h->edges_[n.in_edges_[0]].j_ << ">" << endl;
      }
      foreach(int edge_id, n.in_edges_){
        cerr << "    " << h->edges_[edge_id].rule_->AsString() << endl;
      }
    }
  }
};


/*
Called once from cdec.cc to setup the initial SCFG translation structure backend
*/
SCFGTranslator::SCFGTranslator(const boost::program_options::variables_map& conf) :
  pimpl_(new SCFGTranslatorImpl(conf)) {}

/*
Called for each sentence to perform translation using the SCFG backend
*/
bool SCFGTranslator::TranslateImpl(const string& input,
                               SentenceMetadata* smeta,
                               const vector<double>& weights,
                               Hypergraph* minus_lm_forest) {

  return pimpl_->Translate(input, smeta, weights, minus_lm_forest);
}

/*
Check for grammar pointer in the sentence markup, for use with sentence specific grammars
 */
void SCFGTranslator::ProcessMarkupHintsImpl(const map<string, string>& kv) {
  map<string,string>::const_iterator it = kv.find("grammar");
  if (it != kv.end()) {
    TextGrammar* sentGrammar = new TextGrammar(it->second);
    sentGrammar->SetMaxSpan(pimpl_->max_span_limit);
    sentGrammar->SetGrammarName(it->second);
    pimpl_->AddSupplementalGrammar(GrammarPtr(sentGrammar));
  }
}

void SCFGTranslator::AddSupplementalGrammarFromString(const std::string& grammar) {
  pimpl_->AddSupplementalGrammarFromString(grammar);
}

void SCFGTranslator::AddSupplementalGrammar(GrammarPtr grammar) {
  pimpl_->AddSupplementalGrammar(grammar);
}

void SCFGTranslator::SentenceCompleteImpl() {
  pimpl_->RemoveSupplementalGrammars();
}

std::string SCFGTranslator::GetDecoderType() const {
  return "SCFG";
}

