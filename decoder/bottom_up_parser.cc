//TODO: when using many nonterminals, group passive edges for a span (treat all as a single X for the active items).

//TODO: figure out what cdyer was talking about when he said that having unary rules A->B and B->A, doesn't make cycles appear in result provided rules are sorted in some way (that they typically are)

#include "bottom_up_parser.h"

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

class ActiveChart;
class PassiveChart {
 public:
  PassiveChart(const string& goal,
               const vector<GrammarPtr>& grammars,
               const Lattice& input,
               Hypergraph* forest);
  ~PassiveChart();

  inline const vector<int>& operator()(int i, int j) const { return chart_(i,j); }
  bool Parse();
  inline int size() const { return chart_.width(); }
  inline bool GoalFound() const { return goal_idx_ >= 0; }
  inline int GetGoalIndex() const { return goal_idx_; }

 private:
  void ApplyRules(const int i,
                  const int j,
                  const RuleBin* rules,
                  const Hypergraph::TailNodeVector& tail,
                  const SparseVector<double>& lattice_feats);

  void ApplyRule(const int i,
                 const int j,
                 const TRulePtr& r,
                 const Hypergraph::TailNodeVector& ant_nodes,
                 const SparseVector<double>& lattice_feats);

  void ApplyUnaryRules(const int i, const int j);
  void TopoSortUnaries();

  const vector<GrammarPtr>& grammars_;
  const Lattice& input_;
  Hypergraph* forest_;
  Array2D<vector<int> > chart_;   // chart_(i,j) is the list of nodes derived spanning i,j
  typedef map<int, int> Cat2NodeMap;
  Array2D<Cat2NodeMap> nodemap_;
  vector<ActiveChart*> act_chart_;
  const WordID goal_cat_;    // category that is being searched for at [0,n]
  TRulePtr goal_rule_;
  int goal_idx_;             // index of goal node, if found
  vector<TRulePtr> unaries_; // topologically sorted list of unary rules from all grammars

  static WordID kGOAL;       // [Goal]
};

WordID PassiveChart::kGOAL = 0;

class ActiveChart {
 public:
  ActiveChart(const Hypergraph* hg, const PassiveChart& psv_chart) :
    hg_(hg),
    act_chart_(psv_chart.size(), psv_chart.size()), psv_chart_(psv_chart) {}

  struct ActiveItem {
    ActiveItem(const GrammarIter* g, const Hypergraph::TailNodeVector& a, const SparseVector<double>& lfeats) :
      gptr_(g), ant_nodes_(a), lattice_feats(lfeats) {}
    explicit ActiveItem(const GrammarIter* g) :
      gptr_(g), ant_nodes_(), lattice_feats() {}

    void ExtendTerminal(int symbol, const SparseVector<double>& src_feats, vector<ActiveItem>* out_cell) const {
      if (symbol == kEPS) {
        out_cell->push_back(ActiveItem(gptr_, ant_nodes_, lattice_feats + src_feats));
      } else {
        const GrammarIter* ni = gptr_->Extend(symbol);
        if (ni)
          out_cell->push_back(ActiveItem(ni, ant_nodes_, lattice_feats + src_feats));
      }
    }
    void ExtendNonTerminal(const Hypergraph* hg, int node_index, vector<ActiveItem>* out_cell) const {
      int symbol = hg->nodes_[node_index].cat_;
      const GrammarIter* ni = gptr_->Extend(symbol);
      if (!ni) return;
      Hypergraph::TailNodeVector na(ant_nodes_.size() + 1);
      for (unsigned i = 0; i < ant_nodes_.size(); ++i)
        na[i] = ant_nodes_[i];
      na[ant_nodes_.size()] = node_index;
      out_cell->push_back(ActiveItem(ni, na, lattice_feats));
    }

    const GrammarIter* gptr_;
    Hypergraph::TailNodeVector ant_nodes_;
    SparseVector<double> lattice_feats;
  };

  inline const vector<ActiveItem>& operator()(int i, int j) const { return act_chart_(i,j); }
  void SeedActiveChart(const Grammar& g) {
    int size = act_chart_.width();
    for (int i = 0; i < size; ++i)
      if (g.HasRuleForSpan(i,i,0))
        act_chart_(i,i).push_back(ActiveItem(g.GetRoot()));
  }

  void ExtendActiveItems(int i, int k, int j) {
    //cerr << "  LOOK(" << i << "," << k << ") for completed items in (" << k << "," << j << ")\n";
    vector<ActiveItem>& cell = act_chart_(i,j);
    const vector<ActiveItem>& icell = act_chart_(i,k);
    const vector<int>& idxs = psv_chart_(k, j);
    //if (!idxs.empty()) { cerr << "FOUND IN (" << k << "," << j << ")\n"; }
    for (vector<ActiveItem>::const_iterator di = icell.begin(); di != icell.end(); ++di) {
      for (vector<int>::const_iterator ni = idxs.begin(); ni != idxs.end(); ++ni) {
         di->ExtendNonTerminal(hg_, *ni, &cell);
      }
    }
  }

  void AdvanceDotsForAllItemsInCell(int i, int j, const vector<vector<LatticeArc> >& input) {
    //cerr << "ADVANCE(" << i << "," << j << ")\n";
    for (int k=i+1; k < j; ++k)
      ExtendActiveItems(i, k, j);

    const vector<LatticeArc>& out_arcs = input[j-1];
    for (vector<LatticeArc>::const_iterator ai = out_arcs.begin();
         ai != out_arcs.end(); ++ai) {
      const WordID& f = ai->label;
      const SparseVector<double>& c = ai->features;
      const int& len = ai->dist2next;
      //cerr << "F: " << TD::Convert(f) << "  dest=" << i << "," << (j+len-1) << endl;
      const vector<ActiveItem>& ec = act_chart_(i, j-1);
      //cerr << "    SRC=" << i << "," << (j-1) << " [ec=" << ec.size() << "]" << endl;
      //if (ec.size() > 0) { cerr << "   LC=" << ec[0].lattice_feats << endl; }
      for (vector<ActiveItem>::const_iterator di = ec.begin(); di != ec.end(); ++di)
        di->ExtendTerminal(f, c, &act_chart_(i, j + len - 1));
    }
  }

 private:
  const Hypergraph* hg_;
  Array2D<vector<ActiveItem> > act_chart_;
  const PassiveChart& psv_chart_;
};

PassiveChart::PassiveChart(const string& goal,
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
    unaries_() {
  act_chart_.resize(grammars_.size());
  for (unsigned i = 0; i < grammars_.size(); ++i) {
    act_chart_[i] = new ActiveChart(forest, *this);
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

void PassiveChart::TopoSortUnaries() {
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

void PassiveChart::ApplyRule(const int i,
                             const int j,
                             const TRulePtr& r,
                             const Hypergraph::TailNodeVector& ant_nodes,
                             const SparseVector<double>& lattice_feats) {
  Hypergraph::Edge* new_edge = forest_->AddEdge(r, ant_nodes);
  // cerr << i << " " << j << ": APPLYING RULE: " << r->AsString() << endl;
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
  if (ni == c2n.end()) {
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
}

void PassiveChart::ApplyRules(const int i,
                       const int j,
                       const RuleBin* rules,
                       const Hypergraph::TailNodeVector& tail,
                       const SparseVector<double>& lattice_feats) {
  const int n = rules->GetNumRules();
  //cerr << i << " " << j << ": NUM RULES: " << n << endl;
  for (int k = 0; k < n; ++k) {
    //cerr << i << " " << j << ": R=" << rules->GetIthRule(k)->AsString() << endl;
    ApplyRule(i, j, rules->GetIthRule(k), tail, lattice_feats);
  }
}

void PassiveChart::ApplyUnaryRules(const int i, const int j) {
  const vector<int>& nodes = chart_(i,j);  // reference is important!
  for (unsigned di = 0; di < nodes.size(); ++di) {
    const WordID cat = forest_->nodes_[nodes[di]].cat_;
    for (unsigned ri = 0; ri < unaries_.size(); ++ri) {
      //cerr << "At (" << i << "," << j << "): applying " << unaries_[ri]->AsString() << endl;
      if (unaries_[ri]->f()[0] == cat) {
        //cerr << "  --MATCH\n";
        const Hypergraph::TailNodeVector ant(1, nodes[di]);
        ApplyRule(i, j, unaries_[ri], ant, SparseVector<double>());  // may update nodes
      }
    }
  }
}

bool PassiveChart::Parse() {
  size_t in_size_2 = input_.size() * input_.size();
  forest_->nodes_.reserve(in_size_2 * 2);
  size_t res = min(static_cast<size_t>(2000000), static_cast<size_t>(in_size_2 * 1000));
  forest_->edges_.reserve(res);
  goal_idx_ = -1;
  for (unsigned gi = 0; gi < grammars_.size(); ++gi)
    act_chart_[gi]->SeedActiveChart(*grammars_[gi]);

  if (!SILENT) cerr << "    ";
  for (unsigned l=1; l<input_.size()+1; ++l) {
    if (!SILENT) cerr << '.';
    for (unsigned i=0; i<input_.size() + 1 - l; ++i) {
      unsigned j = i + l;
      for (unsigned gi = 0; gi < grammars_.size(); ++gi) {
        const Grammar& g = *grammars_[gi];
        if (g.HasRuleForSpan(i, j, input_.Distance(i, j))) {
          act_chart_[gi]->AdvanceDotsForAllItemsInCell(i, j, input_);

          const vector<ActiveChart::ActiveItem>& cell = (*act_chart_[gi])(i,j);
          for (vector<ActiveChart::ActiveItem>::const_iterator ai = cell.begin();
               ai != cell.end(); ++ai) {
            const RuleBin* rules = (ai->gptr_->GetRules());
            if (!rules) continue;
            ApplyRules(i, j, rules, ai->ant_nodes_, ai->lattice_feats);
          }
        }
      }
      ApplyUnaryRules(i,j);

      for (unsigned gi = 0; gi < grammars_.size(); ++gi) {
        const Grammar& g = *grammars_[gi];
          // deal with non-terminals that were just proved
          if (g.HasRuleForSpan(i, j, input_.Distance(i,j)))
            act_chart_[gi]->ExtendActiveItems(i, i, j);
      }
    }
    const vector<int>& dh = chart_(0, input_.size());
    for (unsigned di = 0; di < dh.size(); ++di) {
      const Hypergraph::Node& node = forest_->nodes_[dh[di]];
      if (node.cat_ == goal_cat_) {
        Hypergraph::TailNodeVector ant(1, node.id_);
        ApplyRule(0, input_.size(), goal_rule_, ant, SparseVector<double>());
      }
    }
  }
  if (!SILENT) cerr << endl;

  if (GoalFound())
    forest_->PruneUnreachable(forest_->nodes_.size() - 1);
  return GoalFound();
}

PassiveChart::~PassiveChart() {
  for (unsigned i = 0; i < act_chart_.size(); ++i)
    delete act_chart_[i];
}

ExhaustiveBottomUpParser::ExhaustiveBottomUpParser(
    const string& goal_sym,
    const vector<GrammarPtr>& grammars) :
  goal_sym_(goal_sym),
  grammars_(grammars) {}

bool ExhaustiveBottomUpParser::Parse(const Lattice& input,
                                     Hypergraph* forest) const {
  kEPS = TD::Convert("*EPS*");
  PassiveChart chart(goal_sym_, grammars_, input, forest);
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
