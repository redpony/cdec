#include "earley_composer.h"

#include <iostream>
#include <fstream>
#include <map>
#include <queue>
#ifndef HAVE_OLD_CPP
# include <unordered_map>
# include <unordered_set>
#else
# include <tr1/unordered_map>
# include <tr1/unordered_set>
namespace std { using std::tr1::unordered_map; using std::tr1::unordered_multiset; using std::tr1::unordered_set; }
#endif

#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include "fast_lexical_cast.hpp"

#include "phrasetable_fst.h"
#include "sparse_vector.h"
#include "tdict.h"
#include "hg.h"
#include "hg_remove_eps.h"

using namespace std;

// Define the following macro if you want to see lots of debugging output
// when you run the chart parser
#undef DEBUG_CHART_PARSER

// A few constants used by the chart parser ///////////////
static const int kMAX_NODES = 2000000;
static const string kPHRASE_STRING = "X";
static bool constants_need_init = true;
static WordID kUNIQUE_START;
static WordID kPHRASE;
static TRulePtr kX1X2;
static TRulePtr kX1;
static WordID kEPS;
static TRulePtr kEPSRule;

static void InitializeConstants() {
  if (constants_need_init) {
    kPHRASE = TD::Convert(kPHRASE_STRING) * -1;
    kUNIQUE_START = TD::Convert("S") * -1;
    kX1X2.reset(new TRule("[X] ||| [X,1] [X,2] ||| [X,1] [X,2]"));
    kX1.reset(new TRule("[X] ||| [X,1] ||| [X,1]"));
    kEPSRule.reset(new TRule("[X] ||| <eps> ||| <eps>"));
    kEPS = TD::Convert("<eps>");
    constants_need_init = false;
  }
}
////////////////////////////////////////////////////////////

TRulePtr CreateBinaryRule(int lhs, int rhs1, int rhs2) {
  TRule* r = new TRule(*kX1X2);
  r->lhs_ = lhs;
  r->f_[0] = rhs1;
  r->f_[1] = rhs2;
  return TRulePtr(r);
}

TRulePtr CreateUnaryRule(int lhs, int rhs1) {
  TRule* r = new TRule(*kX1);
  r->lhs_ = lhs;
  r->f_[0] = rhs1;
  return TRulePtr(r);
}

TRulePtr CreateEpsilonRule(int lhs) {
  TRule* r = new TRule(*kEPSRule);
  r->lhs_ = lhs;
  return TRulePtr(r);
}

class EGrammarNode {
  friend bool EarleyComposer::Compose(const Hypergraph& src_forest, Hypergraph* trg_forest);
  friend void AddGrammarRule(const string& r, map<WordID, EGrammarNode>* g);
 public:
#ifdef DEBUG_CHART_PARSER
  string hint;
#endif
  EGrammarNode() : is_some_rule_complete(false), is_root(false) {}
  const map<WordID, EGrammarNode>& GetTerminals() const { return tptr; }
  const map<WordID, EGrammarNode>& GetNonTerminals() const { return ntptr; }
  bool HasNonTerminals() const { return (!ntptr.empty()); }
  bool HasTerminals() const { return (!tptr.empty()); }
  bool RuleCompletes() const {
    return (is_some_rule_complete || (ntptr.empty() && tptr.empty()));
  }
  bool GrammarContinues() const {
    return !(ntptr.empty() && tptr.empty());
  }
  bool IsRoot() const {
    return is_root;
  }
  // these are the features associated with the rule from the start
  // node up to this point.  If you use these features, you must
  // not Extend() this rule.
  const SparseVector<double>& GetCFGProductionFeatures() const {
    return input_features;
  }

  const EGrammarNode* Extend(const WordID& t) const {
    if (t < 0) {
      map<WordID, EGrammarNode>::const_iterator it = ntptr.find(t);
      if (it == ntptr.end()) return NULL;
      return &it->second;
    } else {
      map<WordID, EGrammarNode>::const_iterator it = tptr.find(t);
      if (it == tptr.end()) return NULL;
      return &it->second;
    }
  }

 private:
  map<WordID, EGrammarNode> tptr;
  map<WordID, EGrammarNode> ntptr;
  SparseVector<double> input_features;
  bool is_some_rule_complete;
  bool is_root;
};
typedef map<WordID, EGrammarNode> EGrammar;    // indexed by the rule LHS

// edges are immutable once created
struct Edge {
#ifdef DEBUG_CHART_PARSER
  static int id_count;
  const int id;
#endif
  const WordID cat;                   // lhs side of rule proved/being proved
  const EGrammarNode* const dot;      // dot position
  const FSTNode* const q;             // start of span
  const FSTNode* const r;             // end of span
  const Edge* const active_parent;    // back pointer, NULL for PREDICT items
  const Edge* const passive_parent;   // back pointer, NULL for SCAN and PREDICT items
  const TargetPhraseSet* const tps;   // translations
  boost::shared_ptr<SparseVector<double> > features; // features from CFG rule

  bool IsPassive() const {
    // when a rule is completed, this value will be set
    return static_cast<bool>(features);
  }
  bool IsActive() const { return !IsPassive(); }
  bool IsInitial() const {
    return !(active_parent || passive_parent);
  }
  bool IsCreatedByScan() const {
    return active_parent && !passive_parent && !dot->IsRoot();
  }
  bool IsCreatedByPredict() const {
    return dot->IsRoot();
  }
  bool IsCreatedByComplete() const {
    return active_parent && passive_parent;
  }

  // constructor for PREDICT
  Edge(WordID c, const EGrammarNode* d, const FSTNode* q_and_r) :
#ifdef DEBUG_CHART_PARSER
    id(++id_count),
#endif
    cat(c), dot(d), q(q_and_r), r(q_and_r), active_parent(NULL), passive_parent(NULL), tps(NULL) {}
  Edge(WordID c, const EGrammarNode* d, const FSTNode* q_and_r, const Edge* act_parent) :
#ifdef DEBUG_CHART_PARSER
    id(++id_count),
#endif
    cat(c), dot(d), q(q_and_r), r(q_and_r), active_parent(act_parent), passive_parent(NULL), tps(NULL) {}

  // constructors for SCAN
  Edge(WordID c, const EGrammarNode* d, const FSTNode* i, const FSTNode* j,
       const Edge* act_par, const TargetPhraseSet* translations) :
#ifdef DEBUG_CHART_PARSER
    id(++id_count),
#endif
    cat(c), dot(d), q(i), r(j), active_parent(act_par), passive_parent(NULL), tps(translations) {}

  Edge(WordID c, const EGrammarNode* d, const FSTNode* i, const FSTNode* j,
       const Edge* act_par, const TargetPhraseSet* translations,
       const SparseVector<double>& feats) :
#ifdef DEBUG_CHART_PARSER
    id(++id_count),
#endif
    cat(c), dot(d), q(i), r(j), active_parent(act_par), passive_parent(NULL), tps(translations),
    features(new SparseVector<double>(feats)) {}

  // constructors for COMPLETE
  Edge(WordID c, const EGrammarNode* d, const FSTNode* i, const FSTNode* j,
       const Edge* act_par, const Edge *pas_par) :
#ifdef DEBUG_CHART_PARSER
    id(++id_count),
#endif
    cat(c), dot(d), q(i), r(j), active_parent(act_par), passive_parent(pas_par), tps(NULL) {
      assert(pas_par->IsPassive());
      assert(act_par->IsActive());
    }

  Edge(WordID c, const EGrammarNode* d, const FSTNode* i, const FSTNode* j,
       const Edge* act_par, const Edge *pas_par, const SparseVector<double>& feats) :
#ifdef DEBUG_CHART_PARSER
    id(++id_count),
#endif
    cat(c), dot(d), q(i), r(j), active_parent(act_par), passive_parent(pas_par), tps(NULL),
    features(new SparseVector<double>(feats)) {
      assert(pas_par->IsPassive());
      assert(act_par->IsActive());
    }

  // constructor for COMPLETE query
  Edge(const FSTNode* _r) :
#ifdef DEBUG_CHART_PARSER
    id(0),
#endif
    cat(0), dot(NULL), q(NULL),
    r(_r), active_parent(NULL), passive_parent(NULL), tps(NULL) {}
  // constructor for MERGE quere
  Edge(const FSTNode* _q, int) :
#ifdef DEBUG_CHART_PARSER
    id(0),
#endif
    cat(0), dot(NULL), q(_q),
    r(NULL), active_parent(NULL), passive_parent(NULL), tps(NULL) {}
};
#ifdef DEBUG_CHART_PARSER
int Edge::id_count = 0;
#endif

ostream& operator<<(ostream& os, const Edge& e) {
  string type = "PREDICT";
  if (e.IsCreatedByScan())
    type = "SCAN";
  else if (e.IsCreatedByComplete())
    type = "COMPLETE";
  os << "["
#ifdef DEBUG_CHART_PARSER
     << '(' << e.id << ") "
#else
     << '(' << &e << ") "
#endif
     << "q=" << e.q << ", r=" << e.r
     << ", cat="<< TD::Convert(e.cat*-1) << ", dot="
     << e.dot
#ifdef DEBUG_CHART_PARSER
     << e.dot->hint
#endif
     << (e.IsActive() ? ", Active" : ", Passive")
     << ", " << type;
#ifdef DEBUG_CHART_PARSER
  if (e.active_parent) { os << ", act.parent=(" << e.active_parent->id << ')'; }
  if (e.passive_parent) { os << ", psv.parent=(" << e.passive_parent->id << ')'; }
#endif
  if (e.tps) { os << ", tps=" << e.tps; }
  return os << ']';
}

struct Traversal {
  const Edge* const edge;      // result from the active / passive combination
  const Edge* const active;
  const Edge* const passive;
  Traversal(const Edge* me, const Edge* a, const Edge* p) : edge(me), active(a), passive(p) {}
};

struct UniqueTraversalHash {
  size_t operator()(const Traversal* t) const {
    size_t x = 5381;
    x = ((x << 5) + x) ^ reinterpret_cast<size_t>(t->active);
    x = ((x << 5) + x) ^ reinterpret_cast<size_t>(t->passive);
    x = ((x << 5) + x) ^ t->edge->IsActive();
    return x;
  }
};

struct UniqueTraversalEquals {
  size_t operator()(const Traversal* a, const Traversal* b) const {
    return (a->passive == b->passive && a->active == b->active && a->edge->IsActive() == b->edge->IsActive());
  }
};

struct UniqueEdgeHash {
  size_t operator()(const Edge* e) const {
    size_t x = 5381;
    if (e->IsActive()) {
      x = ((x << 5) + x) ^ reinterpret_cast<size_t>(e->dot);
      x = ((x << 5) + x) ^ reinterpret_cast<size_t>(e->q);
      x = ((x << 5) + x) ^ reinterpret_cast<size_t>(e->r);
      x = ((x << 5) + x) ^ static_cast<size_t>(e->cat);
      x += 13;
    } else {  // with passive edges, we don't care about the dot
      x = ((x << 5) + x) ^ reinterpret_cast<size_t>(e->q);
      x = ((x << 5) + x) ^ reinterpret_cast<size_t>(e->r);
      x = ((x << 5) + x) ^ static_cast<size_t>(e->cat);
    }
    return x;
  }
};

struct UniqueEdgeEquals {
  bool operator()(const Edge* a, const Edge* b) const {
    if (a->IsActive() != b->IsActive()) return false;
    if (a->IsActive()) {
      return (a->cat == b->cat) && (a->dot == b->dot) && (a->q == b->q) && (a->r == b->r);
    } else {
      return (a->cat == b->cat) && (a->q == b->q) && (a->r == b->r);
    }
  }
};

struct REdgeHash {
  size_t operator()(const Edge* e) const {
    size_t x = 5381;
    x = ((x << 5) + x) ^ reinterpret_cast<size_t>(e->r);
    return x;
  }
};

struct REdgeEquals {
  bool operator()(const Edge* a, const Edge* b) const {
    return (a->r == b->r);
  }
};

struct QEdgeHash {
  size_t operator()(const Edge* e) const {
    size_t x = 5381;
    x = ((x << 5) + x) ^ reinterpret_cast<size_t>(e->q);
    return x;
  }
};

struct QEdgeEquals {
  bool operator()(const Edge* a, const Edge* b) const {
    return (a->q == b->q);
  }
};

struct EdgeQueue {
  queue<const Edge*> q;
  EdgeQueue() {}
  void clear() { while(!q.empty()) q.pop(); }
  bool HasWork() const { return !q.empty(); }
  const Edge* Next() { const Edge* res = q.front(); q.pop(); return res; }
  void AddEdge(const Edge* s) { q.push(s); }
};

class EarleyComposerImpl {
 public:
  EarleyComposerImpl(WordID start_cat, const FSTNode& q_0) : start_cat_(start_cat), q_0_(&q_0) {}

  // returns false if the intersection is empty
  bool Compose(const EGrammar& g, Hypergraph* forest) {
    goal_node = NULL;
    EGrammar::const_iterator sit = g.find(start_cat_);
    forest->ReserveNodes(kMAX_NODES);
    assert(sit != g.end());
    Edge* init = new Edge(start_cat_, &sit->second, q_0_);
    if (!IncorporateNewEdge(init)) {
      cerr << "Failed to create initial edge!\n";
      abort();
    }
    while (exp_agenda.HasWork() || agenda.HasWork()) {
      while(exp_agenda.HasWork()) {
        const Edge* edge = exp_agenda.Next();
        FinishEdge(edge, forest);
      }
      if (agenda.HasWork()) {
        const Edge* edge = agenda.Next();
#ifdef DEBUG_CHART_PARSER
        cerr << "processing (" << edge->id << ')' << endl;
#endif
        if (edge->IsActive()) {
          if (edge->dot->HasTerminals())
            DoScan(edge);
          if (edge->dot->HasNonTerminals()) {
            DoMergeWithPassives(edge);
            DoPredict(edge, g);
          }
        } else {
          DoComplete(edge);
        }
      }
    }
    if (goal_node) {
      forest->PruneUnreachable(goal_node->id_);
      RemoveEpsilons(forest, kEPS);
    }
    FreeAll();
    return goal_node;
  }

  void FreeAll() {
    for (int i = 0; i < free_list_.size(); ++i)
      delete free_list_[i];
    free_list_.clear();
    for (int i = 0; i < traversal_free_list_.size(); ++i)
      delete traversal_free_list_[i];
    traversal_free_list_.clear();
    all_traversals.clear();
    exp_agenda.clear();
    agenda.clear();
    tps2node.clear();
    edge2node.clear();
    all_edges.clear();
    passive_edges.clear();
    active_edges.clear();
  }

  ~EarleyComposerImpl() {
    FreeAll();
  }

  // returns the total number of edges created during composition
  int EdgesCreated() const {
    return free_list_.size();
  }

 private:
  void DoScan(const Edge* edge) {
    // here, we assume that the FST will potentially have many more outgoing
    // edges than the grammar, which will be just a couple.  If you want to
    // efficiently handle the case where both are relatively large, this code
    // will need to change how the intersection is done.  The best general
    // solution would probably be the Baeza-Yates double binary search.

    const EGrammarNode* dot = edge->dot;
    const FSTNode* r = edge->r;
    const map<WordID, EGrammarNode>& terms = dot->GetTerminals();
    for (map<WordID, EGrammarNode>::const_iterator git = terms.begin();
         git != terms.end(); ++git) {
      const FSTNode* next_r = r->Extend(git->first);
      if (!next_r) continue;
      const EGrammarNode* next_dot = &git->second;
      const bool grammar_continues = next_dot->GrammarContinues();
      const bool rule_completes    = next_dot->RuleCompletes();
      assert(grammar_continues || rule_completes);
      const SparseVector<double>& input_features = next_dot->GetCFGProductionFeatures();
      // create up to 4 new edges!
      if (next_r->HasOutgoingNonEpsilonEdges()) {     // are there further symbols in the FST?
        const TargetPhraseSet* translations = NULL;
        if (rule_completes)
          IncorporateNewEdge(new Edge(edge->cat, next_dot, edge->q, next_r, edge, translations, input_features));
        if (grammar_continues)
          IncorporateNewEdge(new Edge(edge->cat, next_dot, edge->q, next_r, edge, translations));
      }
      if (next_r->HasData()) {   // indicates a loop back to q_0 in the FST
        const TargetPhraseSet* translations = next_r->GetTranslations();
        if (rule_completes)
          IncorporateNewEdge(new Edge(edge->cat, next_dot, edge->q, q_0_, edge, translations, input_features));
        if (grammar_continues)
          IncorporateNewEdge(new Edge(edge->cat, next_dot, edge->q, q_0_, edge, translations));
      }
    }
  }

  void DoPredict(const Edge* edge, const EGrammar& g) {
    const EGrammarNode* dot = edge->dot;
    const map<WordID, EGrammarNode>& non_terms = dot->GetNonTerminals();
    for (map<WordID, EGrammarNode>::const_iterator git = non_terms.begin();
         git != non_terms.end(); ++git) {
      const WordID nt_to_predict = git->first;
      //cerr << edge->id << " -- " << TD::Convert(nt_to_predict*-1) << endl;
      EGrammar::const_iterator egi = g.find(nt_to_predict);
      if (egi == g.end()) {
        cerr << "[ERROR] Can't find any grammar rules with a LHS of type "
             << TD::Convert(-1*nt_to_predict) << '!' << endl;
        continue;
      }
      assert(edge->IsActive());
      const EGrammarNode* new_dot = &egi->second;
      Edge* new_edge = new Edge(nt_to_predict, new_dot, edge->r, edge);
      IncorporateNewEdge(new_edge);
    }
  }

  void DoComplete(const Edge* passive) {
#ifdef DEBUG_CHART_PARSER
    cerr << "  complete: " << *passive << endl;
#endif
    const WordID completed_nt = passive->cat;
    const FSTNode* q = passive->q;
    const FSTNode* next_r = passive->r;
    const Edge query(q);
    const pair<unordered_multiset<const Edge*, REdgeHash, REdgeEquals>::iterator,
         unordered_multiset<const Edge*, REdgeHash, REdgeEquals>::iterator > p =
      active_edges.equal_range(&query);
    for (unordered_multiset<const Edge*, REdgeHash, REdgeEquals>::iterator it = p.first;
         it != p.second; ++it) {
      const Edge* active = *it;
#ifdef DEBUG_CHART_PARSER
      cerr << "    pos: " << *active << endl;
#endif
      const EGrammarNode* next_dot = active->dot->Extend(completed_nt);
      if (!next_dot) continue;
      const SparseVector<double>& input_features = next_dot->GetCFGProductionFeatures();
      // add up to 2 rules
      if (next_dot->RuleCompletes())
        IncorporateNewEdge(new Edge(active->cat, next_dot, active->q, next_r, active, passive, input_features));
      if (next_dot->GrammarContinues())
        IncorporateNewEdge(new Edge(active->cat, next_dot, active->q, next_r, active, passive));
    }
  }

  void DoMergeWithPassives(const Edge* active) {
    // edge is active, has non-terminals, we need to find the passives that can extend it
    assert(active->IsActive());
    assert(active->dot->HasNonTerminals());
#ifdef DEBUG_CHART_PARSER
    cerr << "  merge active with passives: ACT=" << *active << endl;
#endif
    const Edge query(active->r, 1);
    const pair<unordered_multiset<const Edge*, QEdgeHash, QEdgeEquals>::iterator,
         unordered_multiset<const Edge*, QEdgeHash, QEdgeEquals>::iterator > p =
      passive_edges.equal_range(&query);
    for (unordered_multiset<const Edge*, QEdgeHash, QEdgeEquals>::iterator it = p.first;
         it != p.second; ++it) {
      const Edge* passive = *it;
      const EGrammarNode* next_dot = active->dot->Extend(passive->cat);
      if (!next_dot) continue;
      const FSTNode* next_r = passive->r;
      const SparseVector<double>& input_features = next_dot->GetCFGProductionFeatures();
      if (next_dot->RuleCompletes())
        IncorporateNewEdge(new Edge(active->cat, next_dot, active->q, next_r, active, passive, input_features));
      if (next_dot->GrammarContinues())
        IncorporateNewEdge(new Edge(active->cat, next_dot, active->q, next_r, active, passive));
    }
  }

  // take ownership of edge memory, add to various indexes, etc
  // returns true if this edge is new
  bool IncorporateNewEdge(Edge* edge) {
    free_list_.push_back(edge);
    if (edge->passive_parent && edge->active_parent) {
      Traversal* t = new Traversal(edge, edge->active_parent, edge->passive_parent);
      traversal_free_list_.push_back(t);
      if (all_traversals.find(t) != all_traversals.end()) {
        return false;
      } else {
        all_traversals.insert(t);
      }
    }
    exp_agenda.AddEdge(edge);
    return true;
  }

  bool FinishEdge(const Edge* edge, Hypergraph* hg) {
    bool is_new = false;
    if (all_edges.find(edge) == all_edges.end()) {
#ifdef DEBUG_CHART_PARSER
      cerr << *edge << " is NEW\n";
#endif
      all_edges.insert(edge);
      is_new = true;
      if (edge->IsPassive()) passive_edges.insert(edge);
      if (edge->IsActive()) active_edges.insert(edge);
      agenda.AddEdge(edge);
    } else {
#ifdef DEBUG_CHART_PARSER
      cerr << *edge << " is NOT NEW.\n";
#endif
    }
    AddEdgeToTranslationForest(edge, hg);
    return is_new;
  }

  // build the translation forest
  void AddEdgeToTranslationForest(const Edge* edge, Hypergraph* hg) {
    assert(hg->nodes_.size() < kMAX_NODES);
    Hypergraph::Node* tps = NULL;
    // first add any target language rules
    if (edge->tps) {
      Hypergraph::Node*& node = tps2node[(size_t)edge->tps];
      if (!node) {
        // cerr << "Creating phrases for " << edge->tps << endl;
        const vector<TRulePtr>& rules = edge->tps->GetRules();
        node = hg->AddNode(kPHRASE);
        for (int i = 0; i < rules.size(); ++i) {
          Hypergraph::Edge* hg_edge = hg->AddEdge(rules[i], Hypergraph::TailNodeVector());
          hg_edge->feature_values_ += rules[i]->GetFeatureValues();
          hg->ConnectEdgeToHeadNode(hg_edge, node);
        }
      }
      tps = node;
    }
    Hypergraph::Node*& head_node = edge2node[edge];
    if (!head_node)
      head_node = hg->AddNode(edge->cat);
    if (edge->cat == start_cat_ && edge->q == q_0_ && edge->r == q_0_ && edge->IsPassive()) {
      assert(goal_node == NULL || goal_node == head_node);
      goal_node = head_node;
    }
    int rhs1 = 0;
    int rhs2 = 0;
    Hypergraph::TailNodeVector tail;
    SparseVector<double> extra;
    if (edge->IsCreatedByPredict()) {
      // extra.set_value(FD::Convert("predict"), 1);
    } else if (edge->IsCreatedByScan()) {
      tail.push_back(edge2node[edge->active_parent]->id_);
      rhs1 = edge->active_parent->cat;
      if (tps) {
        tail.push_back(tps->id_);
        rhs2 = kPHRASE;
      }
      //extra.set_value(FD::Convert("scan"), 1);
    } else if (edge->IsCreatedByComplete()) {
      tail.push_back(edge2node[edge->active_parent]->id_);
      rhs1 = edge->active_parent->cat;
      tail.push_back(edge2node[edge->passive_parent]->id_);
      rhs2 = edge->passive_parent->cat;
      //extra.set_value(FD::Convert("complete"), 1);
    } else {
      assert(!"unexpected edge type!");
    }
    //cerr << head_node->id_ << "<--" << *edge << endl;

#ifdef DEBUG_CHART_PARSER
      for (int i = 0; i < tail.size(); ++i)
        if (tail[i] == head_node->id_) {
          cerr << "ERROR: " << *edge << "\n   i=" << i << endl;
          if (i == 1) { cerr << "\tP: " << *edge->passive_parent << endl; }
          if (i == 0) { cerr << "\tA: " << *edge->active_parent << endl; }
          assert(!"self-loop found!");
        }
#endif
    Hypergraph::Edge* hg_edge = NULL;
    if (tail.size() == 0) {
      hg_edge = hg->AddEdge(CreateEpsilonRule(edge->cat), tail);
    } else if (tail.size() == 1) {
      hg_edge = hg->AddEdge(CreateUnaryRule(edge->cat, rhs1), tail);
    } else if (tail.size() == 2) {
      hg_edge = hg->AddEdge(CreateBinaryRule(edge->cat, rhs1, rhs2), tail);
    }
    if (edge->features)
      hg_edge->feature_values_ += *edge->features;
    hg_edge->feature_values_ += extra;
    hg->ConnectEdgeToHeadNode(hg_edge, head_node);
  }

  Hypergraph::Node* goal_node;
  EdgeQueue exp_agenda;
  EdgeQueue agenda;
  unordered_map<size_t, Hypergraph::Node*> tps2node;
  unordered_map<const Edge*, Hypergraph::Node*, UniqueEdgeHash, UniqueEdgeEquals> edge2node;
  unordered_set<const Traversal*, UniqueTraversalHash, UniqueTraversalEquals> all_traversals;
  unordered_set<const Edge*, UniqueEdgeHash, UniqueEdgeEquals> all_edges;
  unordered_multiset<const Edge*, QEdgeHash, QEdgeEquals> passive_edges;
  unordered_multiset<const Edge*, REdgeHash, REdgeEquals> active_edges;
  vector<Edge*> free_list_;
  vector<Traversal*> traversal_free_list_;
  const WordID start_cat_;
  const FSTNode* const q_0_;
};

#ifdef DEBUG_CHART_PARSER
static string TrimRule(const string& r) {
  size_t start = r.find(" |||") + 5;
  size_t end = r.rfind(" |||");
  return r.substr(start, end - start);
}
#endif

void AddGrammarRule(const string& r, EGrammar* g) {
  const size_t pos = r.find(" ||| ");
  if (pos == string::npos || r[0] != '[') {
    cerr << "Bad rule: " << r << endl;
    return;
  }
  const size_t rpos = r.rfind(" ||| ");
  string feats;
  string rs = r;
  if (rpos != pos) {
    feats = r.substr(rpos + 5);
    rs = r.substr(0, rpos);
  }
  string rhs = rs.substr(pos + 5);
  string trule = rs + " ||| " + rhs + " ||| " + feats;
  TRule tr(trule);
#ifdef DEBUG_CHART_PARSER
  string hint_last_rule;
#endif
  EGrammarNode* cur = &(*g)[tr.GetLHS()];
  cur->is_root = true;
  for (int i = 0; i < tr.FLength(); ++i) {
    WordID sym = tr.f()[i];
#ifdef DEBUG_CHART_PARSER
    hint_last_rule = TD::Convert(sym < 0 ? -sym : sym);
    cur->hint += " <@@> (*" + hint_last_rule + ") " + TrimRule(tr.AsString());
#endif
    if (sym < 0)
      cur = &cur->ntptr[sym];
    else
      cur = &cur->tptr[sym];
  }
#ifdef DEBUG_CHART_PARSER
  cur->hint += " <@@> (" + hint_last_rule + "*) " + TrimRule(tr.AsString());
#endif
  cur->is_some_rule_complete = true;
  cur->input_features = tr.GetFeatureValues();
}

EarleyComposer::~EarleyComposer() {
  delete pimpl_;
}

EarleyComposer::EarleyComposer(const FSTNode* fst) {
  InitializeConstants();
  pimpl_ = new EarleyComposerImpl(kUNIQUE_START, *fst);
}

bool EarleyComposer::Compose(const Hypergraph& src_forest, Hypergraph* trg_forest) {
  // first, convert the src forest into an EGrammar
  EGrammar g;
  const int nedges = src_forest.edges_.size();
  const int nnodes = src_forest.nodes_.size();
  vector<int> cats(nnodes);
  bool assign_cats = false;
  for (int i = 0; i < nnodes; ++i)
    if (assign_cats) {
      cats[i] = TD::Convert("CAT_" + boost::lexical_cast<string>(i)) * -1;
    } else {
      cats[i] = src_forest.nodes_[i].cat_;
    }
  // construct the grammar
  for (int i = 0; i < nedges; ++i) {
    const Hypergraph::Edge& edge = src_forest.edges_[i];
    const vector<WordID>& src = edge.rule_->f();
    EGrammarNode* cur = &g[cats[edge.head_node_]];
    cur->is_root = true;
    int ntc = 0;
    for (int j = 0; j < src.size(); ++j) {
      WordID sym = src[j];
      if (sym <= 0) {
        sym = cats[edge.tail_nodes_[ntc]];
        ++ntc;
        cur = &cur->ntptr[sym];
      } else {
        cur = &cur->tptr[sym];
      }
    }
    cur->is_some_rule_complete = true;
    cur->input_features = edge.feature_values_;
  }
  EGrammarNode& goal_rule = g[kUNIQUE_START];
  assert((goal_rule.ntptr.size() == 1 && goal_rule.tptr.size() == 0) ||
         (goal_rule.ntptr.size() == 0 && goal_rule.tptr.size() == 1));

  return pimpl_->Compose(g, trg_forest);
}

bool EarleyComposer::Compose(istream* in, Hypergraph* trg_forest) {
  EGrammar g;
  while(*in) {
    string line;
    getline(*in, line);
    if (line.empty()) continue;
    AddGrammarRule(line, &g);
  }

  return pimpl_->Compose(g, trg_forest);
}
