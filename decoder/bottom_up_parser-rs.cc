#include "bottom_up_parser-rs.h"

#include <iostream>
#include <map>

#include "node_state_hash.h"
#include "nt_span.h"
#include "hg.h"
#include "array2d.h"
#include "tdict.h"
#include "verbose.h"

using namespace std;

static WordID kEPS = 0;

struct RSActiveItem;
class RSChart {
 public:
  RSChart(const string& goal,
               const vector<GrammarPtr>& grammars,
               const Lattice& input,
               Hypergraph* forest);
  ~RSChart();

  void AddToChart(const RSActiveItem& x, int i, int j);
  void ConsumeTerminal(const RSActiveItem& x, int i, int j, int k);
  void ConsumeNonTerminal(const RSActiveItem& x, int i, int j, int k);
  bool Parse();
  inline bool GoalFound() const { return goal_idx_ >= 0; }
  inline int GetGoalIndex() const { return goal_idx_; }

 private:
  void ApplyRules(const int i,
                  const int j,
                  const RuleBin* rules,
                  const Hypergraph::TailNodeVector& tail,
                  const SparseVector<double>& lattice_feats);

  // returns true if a new node was added to the chart
  // false otherwise
  bool ApplyRule(const int i,
                 const int j,
                 const TRulePtr& r,
                 const Hypergraph::TailNodeVector& ant_nodes,
                 const SparseVector<double>& lattice_feats);

  void ApplyUnaryRules(const int i, const int j, const WordID& cat, unsigned nodeidx);
  void TopoSortUnaries();

  const vector<GrammarPtr>& grammars_;
  const Lattice& input_;
  Hypergraph* forest_;
  Array2D<vector<int>> chart_;   // chart_(i,j) is the list of nodes (represented
                                 // by their index in forest_->nodes_) derived spanning i,j
  typedef map<int, int> Cat2NodeMap;
  Array2D<Cat2NodeMap> nodemap_;
  const WordID goal_cat_;    // category that is being searched for at [0,n]
  TRulePtr goal_rule_;
  int goal_idx_;             // index of goal node, if found
  const int lc_fid_;
  vector<TRulePtr> unaries_; // topologically sorted list of unary rules from all grammars

  static WordID kGOAL;       // [Goal]
};

WordID RSChart::kGOAL = 0;

// "a type-2 is identified by a trie node, an array of back-pointers to antecedent cells, and a span"
struct RSActiveItem {
  explicit RSActiveItem(const GrammarIter* g, int i) :
    gptr_(g), ant_nodes_(), lattice_feats(), i_(i) {}
  void ExtendTerminal(int symbol, const SparseVector<double>& src_feats) {
    lattice_feats += src_feats;
    if (symbol != kEPS)
      gptr_ = gptr_->Extend(symbol);
  }
  void ExtendNonTerminal(const Hypergraph* hg, int node_index) {
    gptr_ = gptr_->Extend(hg->nodes_[node_index].cat_);
    ant_nodes_.push_back(node_index);
  }
  // returns false if the extension has failed
  explicit operator bool() const {
    return gptr_;
  }
  const GrammarIter* gptr_;
  Hypergraph::TailNodeVector ant_nodes_;
  SparseVector<double> lattice_feats;
  short i_;
};

// some notes on the implementation
// "X" in Rico's Algorithm 2 roughly looks like it is just a pointer into a grammar
// trie, but it is actually a full "dotted item" since it needs to contain the information
// to build the hypergraph (i.e., it must remember the antecedent nodes and where they are,
// also any information about the path costs).

RSChart::RSChart(const string& goal,
                           const vector<GrammarPtr>& grammars,
                           const Lattice& input,
                           Hypergraph* forest) :
    grammars_(grammars),
    input_(input),
    forest_(forest),
    chart_(input.size()+1, input.size()+1),
    nodemap_(input.size()+1, input.size()+1),
    goal_cat_(TD::Convert(goal) * -1),
    goal_rule_(new TRule("[Goal] ||| [" + goal + "] ||| [1]")),
    goal_idx_(-1),
    lc_fid_(FD::Convert("LatticeCost")),
    unaries_() {
  for (unsigned i = 0; i < grammars_.size(); ++i) {
    const vector<TRulePtr>& u = grammars_[i]->GetAllUnaryRules();
    for (unsigned j = 0; j < u.size(); ++j)
      unaries_.push_back(u[j]);
  }
  TopoSortUnaries();
  if (!kGOAL) kGOAL = TD::Convert("Goal") * -1;
  if (!SILENT) cerr << "  Goal category: [" << goal << ']' << endl;
}

static bool TopoSortVisit(int node, vector<TRulePtr>& u, const map<int, vector<TRulePtr> >& g, map<int, int>& mark) {
  if (mark[node] == 1) {
    cerr << "[ERROR] Unary rule cycle detected involving [" << TD::Convert(-node) << "]\n";
    return false; // cycle detected
  } else if (mark[node] == 2) {
    return true; // already been 
  }
  mark[node] = 1;
  const map<int, vector<TRulePtr> >::const_iterator nit = g.find(node);
  if (nit != g.end()) {
    const vector<TRulePtr>& edges = nit->second;
    vector<bool> okay(edges.size(), true);
    for (unsigned i = 0; i < edges.size(); ++i) {
      okay[i] = TopoSortVisit(edges[i]->lhs_, u, g, mark);
      if (!okay[i]) {
        cerr << "[ERROR] Unary rule cycle detected, removing: " << edges[i]->AsString() << endl;
      }
   }
    for (unsigned i = 0; i < edges.size(); ++i) {
      if (okay[i]) u.push_back(edges[i]);
      //if (okay[i]) cerr << "UNARY: " << edges[i]->AsString() << endl;
    }
  }
  mark[node] = 2;
  return true;
}

void RSChart::TopoSortUnaries() {
  vector<TRulePtr> u(unaries_.size()); u.clear();
  map<int, vector<TRulePtr> > g;
  map<int, int> mark;
  //cerr << "GOAL=" << TD::Convert(-goal_cat_) << endl;
  mark[goal_cat_] = 2;
  for (unsigned i = 0; i < unaries_.size(); ++i) {
    //cerr << "Adding: " << unaries_[i]->AsString() << endl;
    g[unaries_[i]->f()[0]].push_back(unaries_[i]);
  }
    //m[unaries_[i]->lhs_].push_back(unaries_[i]);
  for (map<int, vector<TRulePtr> >::iterator it = g.begin(); it != g.end(); ++it) {
    //cerr << "PROC: " << TD::Convert(-it->first) << endl;
    if (mark[it->first] > 0) {
      //cerr << "Already saw [" << TD::Convert(-it->first) << "]\n";
    } else {
      TopoSortVisit(it->first, u, g, mark);
    }
  }
  unaries_.clear();
  for (int i = u.size() - 1; i >= 0; --i)
    unaries_.push_back(u[i]);
}

bool RSChart::ApplyRule(const int i,
                        const int j,
                        const TRulePtr& r,
                        const Hypergraph::TailNodeVector& ant_nodes,
                        const SparseVector<double>& lattice_feats) {
  Hypergraph::Edge* new_edge = forest_->AddEdge(r, ant_nodes);
  //cerr << i << " " << j << ": APPLYING RULE: " << r->AsString() << endl;
  new_edge->prev_i_ = r->prev_i;
  new_edge->prev_j_ = r->prev_j;
  new_edge->i_ = i;
  new_edge->j_ = j;
  new_edge->feature_values_ = r->GetFeatureValues();
  new_edge->feature_values_ += lattice_feats;
  Cat2NodeMap& c2n = nodemap_(i,j);
  const bool is_goal = (r->GetLHS() == kGOAL);
  const Cat2NodeMap::iterator ni = c2n.find(r->GetLHS());
  Hypergraph::Node* node = NULL;
  bool added_node = false;
  if (ni == c2n.end()) {
    //cerr << "(" << i << "," << j << ") => " << TD::Convert(-r->GetLHS()) << endl;
    added_node = true;
    node = forest_->AddNode(r->GetLHS());
    c2n[r->GetLHS()] = node->id_;
    if (is_goal) {
      assert(goal_idx_ == -1);
      goal_idx_ = node->id_;
    } else {
      chart_(i,j).push_back(node->id_);
    }
  } else {
    node = &forest_->nodes_[ni->second];
  }
  forest_->ConnectEdgeToHeadNode(new_edge, node);
  return added_node;
}

void RSChart::ApplyRules(const int i,
                         const int j,
                         const RuleBin* rules,
                         const Hypergraph::TailNodeVector& tail,
                         const SparseVector<double>& lattice_feats) {
  const int n = rules->GetNumRules();
  //cerr << i << " " << j << ": NUM RULES: " << n << endl;
  for (int k = 0; k < n; ++k) {
    //cerr << i << " " << j << ": R=" << rules->GetIthRule(k)->AsString() << endl;
    TRulePtr rule = rules->GetIthRule(k);
    // apply rule, and if we create a new node, apply any necessary
    // unary rules
    if (ApplyRule(i, j, rule, tail, lattice_feats)) {
      unsigned nodeidx = nodemap_(i,j)[rule->lhs_];
      ApplyUnaryRules(i, j, rule->lhs_, nodeidx);
    }
  }
}

void RSChart::ApplyUnaryRules(const int i, const int j, const WordID& cat, unsigned nodeidx) {
  for (unsigned ri = 0; ri < unaries_.size(); ++ri) {
    //cerr << "At (" << i << "," << j << "): applying " << unaries_[ri]->AsString() << endl;
    if (unaries_[ri]->f()[0] == cat) {
      //cerr << "  --MATCH\n";
      WordID new_lhs = unaries_[ri]->GetLHS();
      const Hypergraph::TailNodeVector ant(1, nodeidx);
      if (ApplyRule(i, j, unaries_[ri], ant, SparseVector<double>())) {
        //cerr << "(" << i << "," << j << ") " << TD::Convert(-cat) << " ---> " << TD::Convert(-new_lhs) << endl;
        unsigned nodeidx = nodemap_(i,j)[new_lhs];
        ApplyUnaryRules(i, j, new_lhs, nodeidx);
      }
    }
  }
}

void RSChart::AddToChart(const RSActiveItem& x, int i, int j) {
  // deal with completed rules
  const RuleBin* rb = x.gptr_->GetRules();
  if (rb) ApplyRules(i, j, rb, x.ant_nodes_, x.lattice_feats);

  //cerr << "Rules applied ... looking for extensions to consume for span (" << i << "," << j << ")\n";
  // continue looking for extensions of the rule to the right
  for (unsigned k = j+1; k <= input_.size(); ++k) {
    ConsumeTerminal(x, i, j, k);
    ConsumeNonTerminal(x, i, j, k);
  }
}

void RSChart::ConsumeTerminal(const RSActiveItem& x, int i, int j, int k) {
  //cerr << "ConsumeT(" << i << "," << j << "," << k << "):\n";
  
  const unsigned check_edge_len = k - j;
  // long-term TODO preindex this search so i->len->words is constant time rather than fan out
  for (auto& in_edge : input_[j]) {
    if (in_edge.dist2next == check_edge_len) {
      //cerr << "  Found word spanning (" << j << "," << k << ") in input, symbol=" << TD::Convert(in_edge.label) << endl;
      RSActiveItem copy = x;
      copy.ExtendTerminal(in_edge.label, in_edge.features);
      if (copy) AddToChart(copy, i, k);
    }
  }
}

void RSChart::ConsumeNonTerminal(const RSActiveItem& x, int i, int j, int k) {
  //cerr << "ConsumeNT(" << i << "," << j << "," << k << "):\n";
  for (auto& nodeidx : chart_(j,k)) {
    //cerr << "  Found completed NT in (" << j << "," << k << ") of type " << TD::Convert(-forest_->nodes_[nodeidx].cat_) << endl;
    RSActiveItem copy = x;
    copy.ExtendNonTerminal(forest_, nodeidx);
    if (copy) AddToChart(copy, i, k);
  }
}

bool RSChart::Parse() {
  size_t in_size_2 = input_.size() * input_.size();
  forest_->nodes_.reserve(in_size_2 * 2);
  size_t res = min(static_cast<size_t>(2000000), static_cast<size_t>(in_size_2 * 1000));
  forest_->edges_.reserve(res);
  goal_idx_ = -1;
  const int N = input_.size();
  for (int i = N - 1; i >= 0; --i) {
    for (int j = i + 1; j <= N; ++j) {
      for (unsigned gi = 0; gi < grammars_.size(); ++gi) {
        RSActiveItem item(grammars_[gi]->GetRoot(), i);
        ConsumeTerminal(item, i, i, j);
      }
      for (unsigned gi = 0; gi < grammars_.size(); ++gi) {
        RSActiveItem item(grammars_[gi]->GetRoot(), i);
        ConsumeNonTerminal(item, i, i, j);
      }
    }
  }

  // look for goal
  const vector<int>& dh = chart_(0, input_.size());
  for (unsigned di = 0; di < dh.size(); ++di) {
    const Hypergraph::Node& node = forest_->nodes_[dh[di]];
    if (node.cat_ == goal_cat_) {
      Hypergraph::TailNodeVector ant(1, node.id_);
      ApplyRule(0, input_.size(), goal_rule_, ant, SparseVector<double>());
    }
  }
  if (!SILENT) cerr << endl;

  if (GoalFound())
    forest_->PruneUnreachable(forest_->nodes_.size() - 1);
  return GoalFound();
}

RSChart::~RSChart() {}

RSExhaustiveBottomUpParser::RSExhaustiveBottomUpParser(
    const string& goal_sym,
    const vector<GrammarPtr>& grammars) :
  goal_sym_(goal_sym),
  grammars_(grammars) {}

bool RSExhaustiveBottomUpParser::Parse(const Lattice& input,
                                     Hypergraph* forest) const {
  kEPS = TD::Convert("*EPS*");
  RSChart chart(goal_sym_, grammars_, input, forest);
  const bool result = chart.Parse();

  if (result) {
    for (auto& node : forest->nodes_) {
      Span prev;
      const Span s = forest->NodeSpan(node.id_, &prev);
      node.node_hash = cdec::HashNode(node.cat_, s.l, s.r, prev.l, prev.r);
    }
  }
  return result;
}
