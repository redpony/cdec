#include "hg_intersect.h"

#include <vector>
#ifndef HAVE_OLD_CPP
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std { using std::tr1::unordered_map; }
#endif
#include "fast_lexical_cast.hpp"
#include <boost/functional/hash.hpp>

#include "verbose.h"
#include "tdict.h"
#include "hg.h"
#include "trule.h"
#include "wordid.h"
#include "bottom_up_parser.h"

using boost::lexical_cast;
using namespace std;

struct RuleFilter {
  unordered_map<vector<WordID>, bool, boost::hash<vector<WordID> > > exists_;
  bool true_lattice;
  RuleFilter(const Lattice& target, unsigned max_phrase_size) {
    true_lattice = false;
    for (unsigned i = 0; i < target.size(); ++i) {
      vector<WordID> phrase;
      const unsigned lim = min(static_cast<unsigned>(target.size()), i + max_phrase_size);
      for (unsigned j = i; j < lim; ++j) {
        if (target[j].size() > 1) { true_lattice = true; break; }
        phrase.push_back(target[j][0].label);
        exists_[phrase] = true;
      }
    }
    vector<WordID> sos(1, TD::Convert("<s>"));
    exists_[sos] = true;
  }
  bool operator()(const TRule& r) const {
    // TODO do some smarter filtering for lattices
    if (true_lattice) return false;  // don't filter "true lattice" input
    const vector<WordID>& e = r.e();
    for (unsigned i = 0; i < e.size(); ++i) {
      if (e[i] <= 0) continue;
      vector<WordID> phrase;
      for (unsigned j = i; j < e.size(); ++j) {
        if (e[j] <= 0) break;
        phrase.push_back(e[j]);
        if (exists_.count(phrase) == 0) return true;
      }
    }
    return false;
  }
};

static bool FastLinearIntersect(const Lattice& target, Hypergraph* hg) {
  if (!SILENT) cerr << "  Fast linear-chain intersection...\n";
  vector<bool> prune(hg->edges_.size(), false);
  set<int> cov;
  map<const TRule*, TRulePtr> inverted_rules;
  for (unsigned i = 0; i < prune.size(); ++i) {
    Hypergraph::Edge& edge = hg->edges_[i];
    if (edge.Arity() == 0) {
      const int trg_index = edge.prev_i_;
      const WordID trg = target[trg_index][0].label;
      assert(edge.rule_->EWords() == 1);
      TRulePtr& inv_rule = inverted_rules[edge.rule_.get()];
      if (!inv_rule) {
        inv_rule.reset(new TRule(*edge.rule_));
        inv_rule->e_.swap(inv_rule->f_);
      }
      prune[i] = (edge.rule_->e_[0] != trg);
      if (!prune[i]) {
        cov.insert(trg_index);
        swap(edge.prev_i_, edge.i_);
        swap(edge.prev_j_, edge.j_);
        edge.rule_.swap(inv_rule);
      }
    }
  }
  hg->PruneEdges(prune, true);
  return (cov.size() == target.size());
}

namespace HG {

bool Intersect(const Lattice& target, Hypergraph* hg) {
  // there are a number of faster algorithms available for restricted
  // classes of hypergraph and/or target.
  if (hg->IsLinearChain() && target.IsSentence())
    return FastLinearIntersect(target, hg);

  vector<bool> rem(hg->edges_.size(), false);
  const RuleFilter filter(target, 15);   // TODO make configurable
  for (unsigned i = 0; i < rem.size(); ++i)
    rem[i] = filter(*hg->edges_[i].rule_);
  hg->PruneEdges(rem, true);

  const unsigned nedges = hg->edges_.size();
  const unsigned nnodes = hg->nodes_.size();

  TextGrammar* g = new TextGrammar;
  GrammarPtr gp(g);
  vector<int> cats(nnodes);
  // each node in the translation forest becomes a "non-terminal" in the new
  // grammar, create the labels here
  const string kSEP = "_";
  for (unsigned i = 0; i < nnodes; ++i) {
    string pstr = "CAT";
    if (hg->nodes_[i].cat_ < 0)
      pstr = TD::Convert(-hg->nodes_[i].cat_);
    cats[i] = TD::Convert(pstr + kSEP + lexical_cast<string>(i)) * -1;
  }

  // construct the grammar
  for (unsigned i = 0; i < nedges; ++i) {
    const Hypergraph::Edge& edge = hg->edges_[i];
    const vector<WordID>& tgt = edge.rule_->e();
    const vector<WordID>& src = edge.rule_->f();
    TRulePtr rule(new TRule);
    rule->prev_i = edge.i_;
    rule->prev_j = edge.j_;
    rule->lhs_ = cats[edge.head_node_];
    vector<WordID>& f = rule->f_;
    vector<WordID>& e = rule->e_;
    f.resize(tgt.size());   // swap source and target, since the parser
    e.resize(src.size());   // parses using the source side!
    Hypergraph::TailNodeVector tn(edge.tail_nodes_.size());
    int ntc = 0;
    for (unsigned j = 0; j < tgt.size(); ++j) {
      const WordID& cur = tgt[j];
      if (cur > 0) {
        f[j] = cur;
      } else {
        tn[-cur] = -ntc;
        ++ntc;
        f[j] = cats[edge.tail_nodes_[-cur]];
      }
    }
    ntc = 0;
    for (unsigned j = 0; j < src.size(); ++j) {
      const WordID& cur = src[j];
      if (cur > 0) {
        e[j] = cur;
      } else {
        e[j] = tn[ntc++];
      }
    }
    rule->scores_ = edge.feature_values_;
    rule->parent_rule_ = edge.rule_;
    rule->ComputeArity();
    //cerr << "ADD: " << rule->AsString() << endl;

    g->AddRule(rule);
  }
  g->SetMaxSpan(target.size() + 1);
  const string& new_goal = TD::Convert(cats.back() * -1);
  vector<GrammarPtr> grammars(1, gp);
  Hypergraph tforest;
  ExhaustiveBottomUpParser parser(new_goal, grammars);
  if (!parser.Parse(target, &tforest))
    return false;
  else
    hg->swap(tforest);
  return true;
}

}

