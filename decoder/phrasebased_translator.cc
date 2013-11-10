#include "phrasebased_translator.h"

#include <queue>
#include <iostream>
#ifndef HAVE_OLD_CPP
# include <unordered_map>
# include <unordered_set>
#else
# include <tr1/unordered_map>
# include <tr1/unordered_set>
namespace std { using std::tr1::unordered_map; using std::tr1::unordered_set; }
#endif

#include <boost/tuple/tuple.hpp>
#include <boost/functional/hash.hpp>

#include "sentence_metadata.h"
#include "tdict.h"
#include "hg.h"
#include "filelib.h"
#include "lattice.h"
#include "phrasetable_fst.h"
#include "array2d.h"

using namespace std;
using namespace boost::tuples;

struct Coverage : public vector<bool> {
  explicit Coverage(int n, bool v = false) : vector<bool>(n, v), first_gap() {}
  void Cover(int i, int j) {
    vector<bool>::iterator it = this->begin() + i;
    vector<bool>::iterator end = this->begin() + j;
    while (it != end)
      *it++ = true;
    if (first_gap == i) {
      first_gap = j;
      it = end;
      while (*it && it != this->end()) {
        ++it;
        ++first_gap;
      }
    }
  }
  bool Collides(int i, int j) const {
    vector<bool>::const_iterator it = this->begin() + i;
    vector<bool>::const_iterator end = this->begin() + j;
    while (it != end)
      if (*it++) return true;
    return false;
  }
  int GetFirstGap() const { return first_gap; }
 private:
  int first_gap;
};
struct CoverageHash {
  size_t operator()(const Coverage& cov) const {
    int seed = 131;
    size_t res = 0;
    for (vector<bool>::const_iterator it = cov.begin(); it != cov.end(); ++it) {
      res = (res * seed) + (*it + 1);
    }
    return res;
  }
};
ostream& operator<<(ostream& os, const Coverage& cov) {
  os << '[';
  for (int i = 0; i < cov.size(); ++i)
    os << (cov[i] ? '*' : '.');
  return os << " gap=" << cov.GetFirstGap() << ']';
}

typedef unordered_map<Coverage, int, CoverageHash> CoverageNodeMap;
typedef unordered_set<Coverage, CoverageHash> UniqueCoverageSet;

struct PhraseBasedTranslatorImpl {
  PhraseBasedTranslatorImpl(const boost::program_options::variables_map& conf) :
      add_pass_through_rules(conf.count("add_pass_through_rules")),
      max_distortion(conf["pb_max_distortion"].as<int>()),
      kCONCAT_RULE(new TRule("[X] ||| [X,1] [X,2] ||| [X,1] [X,2]", true)),
      kNT_TYPE(TD::Convert("X") * -1) {
    assert(max_distortion >= 0);
    vector<string> gfiles = conf["grammar"].as<vector<string> >();
    assert(gfiles.size() == 1);
    cerr << "Reading phrasetable from " << gfiles.front() << endl;
    ReadFile in(gfiles.front());
    fst.reset(LoadTextPhrasetable(in.stream()));
  }

  struct State {
    State(const Coverage& c, int _i, int _j, const FSTNode* q) :
      coverage(c), i(_i), j(_j), fst(q) {}
    Coverage coverage;
    int i;
    int j;
    const FSTNode* fst;
  };

  // we keep track of unique coverages that have been extended since it's
  // possible to "extend" the same coverage twice, e.g. translate "a b c"
  // with phrases "a" "b" "a b" and "c".  There are two ways to cover "a b"
  void EnqueuePossibleContinuations(const Coverage& coverage, queue<State>* q, UniqueCoverageSet* ucs) {
    if (ucs->insert(coverage).second) {
      const int gap = coverage.GetFirstGap();
      const int end = min(static_cast<int>(coverage.size()), gap + max_distortion + 1);
      for (int i = gap; i < end; ++i)
        if (!coverage[i]) q->push(State(coverage, i, i, fst.get()));
    }
  }

  bool Translate(const std::string& input,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* minus_lm_forest) {
    Lattice lattice;
    LatticeTools::ConvertTextOrPLF(input, &lattice);
    smeta->SetSourceLength(lattice.size());
    size_t est_nodes = lattice.size() * lattice.size() * (1 << max_distortion);
    minus_lm_forest->ReserveNodes(est_nodes, est_nodes * 100);
    if (add_pass_through_rules) {
      SparseVector<double> feats;
      feats.set_value(FD::Convert("PassThrough"), 1);
      for (int i = 0; i < lattice.size(); ++i) {
        const vector<LatticeArc>& arcs = lattice[i];
        for (int j = 0; j < arcs.size(); ++j) {
          fst->AddPassThroughTranslation(arcs[j].label, feats);
          // TODO handle lattice edge features
        }
      }
    }
    CoverageNodeMap c;
    queue<State> q;
    UniqueCoverageSet ucs;
    const Coverage empty_cov(lattice.size(), false);
    const Coverage goal_cov(lattice.size(), true);
    EnqueuePossibleContinuations(empty_cov, &q, &ucs);
    c[empty_cov] = 0;   // have to handle the left edge specially
    while(!q.empty()) {
      const State s = q.front();
      q.pop();
      // cerr << "(" << s.i << "," << s.j << " ptr=" << s.fst << ") cov=" << s.coverage << endl;
      const vector<LatticeArc>& arcs = lattice[s.j];
      if (s.fst->HasData()) {
        Coverage new_cov = s.coverage;
        new_cov.Cover(s.i, s.j);
        EnqueuePossibleContinuations(new_cov, &q, &ucs);
        const vector<TRulePtr>& phrases = s.fst->GetTranslations()->GetRules();
        const int phrase_head_index = minus_lm_forest->AddNode(kNT_TYPE)->id_;
        for (int i = 0; i < phrases.size(); ++i) {
          Hypergraph::Edge* edge = minus_lm_forest->AddEdge(phrases[i], Hypergraph::TailNodeVector());
          edge->feature_values_ = edge->rule_->scores_;
          edge->i_ = s.i;
          edge->j_ = s.j;
          minus_lm_forest->ConnectEdgeToHeadNode(edge->id_, phrase_head_index);
        }
        CoverageNodeMap::iterator cit = c.find(s.coverage);
        assert(cit != c.end());
        const int tail_node_plus1 = cit->second;
        if (tail_node_plus1 == 0) {  // left edge
          c[new_cov] = phrase_head_index + 1;
        } else { // not left edge
          int& head_node_plus1 = c[new_cov];
          if (!head_node_plus1)
            head_node_plus1 = minus_lm_forest->AddNode(kNT_TYPE)->id_ + 1;
          Hypergraph::TailNodeVector tail(2, tail_node_plus1 - 1);
          tail[1] = phrase_head_index;
          const int concat_edge = minus_lm_forest->AddEdge(kCONCAT_RULE, tail)->id_;
          minus_lm_forest->ConnectEdgeToHeadNode(concat_edge, head_node_plus1 - 1);
        }
      }
      if (s.j == lattice.size()) continue;
      for (int l = 0; l < arcs.size(); ++l) {
        const LatticeArc& arc = arcs[l];

        const FSTNode* next_fst_state = s.fst->Extend(arc.label);
        const int next_j = s.j + arc.dist2next;
        if (next_fst_state &&
            !s.coverage.Collides(s.i, next_j)) {
          q.push(State(s.coverage, s.i, next_j, next_fst_state));
        }
      }
    }
    if (add_pass_through_rules)
      fst->ClearPassThroughTranslations();
    int pregoal_plus1 = c[goal_cov];
    if (pregoal_plus1 > 0) {
      TRulePtr kGOAL_RULE(new TRule("[Goal] ||| [X,1] ||| [X,1]"));
      int goal = minus_lm_forest->AddNode(TD::Convert("Goal") * -1)->id_;
      int gedge = minus_lm_forest->AddEdge(kGOAL_RULE, Hypergraph::TailNodeVector(1, pregoal_plus1 - 1))->id_;
      minus_lm_forest->ConnectEdgeToHeadNode(gedge, goal);
      // they are almost topo, but not quite always
      minus_lm_forest->TopologicallySortNodesAndEdges(goal);
      minus_lm_forest->Reweight(weights);
      return true;
    } else {
      return false;  // composition failed
    }
  }

  const bool add_pass_through_rules;
  const int max_distortion;
  const TRulePtr kCONCAT_RULE;
  const WordID kNT_TYPE;
  boost::shared_ptr<FSTNode> fst;
};

PhraseBasedTranslator::PhraseBasedTranslator(const boost::program_options::variables_map& conf) :
  pimpl_(new PhraseBasedTranslatorImpl(conf)) {}

bool PhraseBasedTranslator::TranslateImpl(const std::string& input,
                                      SentenceMetadata* smeta,
                                      const std::vector<double>& weights,
                                      Hypergraph* minus_lm_forest) {
  return pimpl_->Translate(input, smeta, weights, minus_lm_forest);
}
