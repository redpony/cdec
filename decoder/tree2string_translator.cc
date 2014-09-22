#include <algorithm>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <boost/shared_ptr.hpp>
#include <boost/functional/hash.hpp>
#include "fast_lexical_cast.hpp"
#include "tree_fragment.h"
#include "translator.h"
#include "hg.h"
#include "sentence_metadata.h"
#include "filelib.h"
#include "stringlib.h"
#include "tdict.h"
#include "verbose.h"

using namespace std;

struct Tree2StringGrammarNode {
  map<unsigned, Tree2StringGrammarNode> next;
  vector<TRulePtr> rules;
};

// this needs to be rewritten so it is fast and checks errors well
// use a lexer probably
static void ReadTree2StringGrammar(istream* in, Tree2StringGrammarNode* root, bool has_multiple_states) {
  string line;
  int lc = 0;
  while(getline(*in, line)) {
    ++lc;
    if (line.size() == 0 || line[0] == '#') continue;
    std::vector<StringPiece> fields = TokenizeMultisep(line, " ||| ");
    if (has_multiple_states && fields.size() != 4) {
      cerr << "Expected 4 fields in rule file but line " << lc << " is:\n" << line << endl;
      abort();
    }
    if (!has_multiple_states && fields.size() != 3) {
      cerr << "Expected 3 fields in rule file but line " << lc << " is:\n" << line << endl;
      abort();
    }
    
    cdec::TreeFragment rule_src(fields[has_multiple_states ? 1 : 0], true);
    // TODO transducer_state should be read from input
    const unsigned transducer_state = 0;
    Tree2StringGrammarNode* cur = &root->next[transducer_state];
    ostringstream os;
    int lhs = -(rule_src.root & cdec::ALL_MASK);
    // build source RHS for SCFG projection
    vector<int> frhs;
    // we traverse the rule_src in left to right, DFS order
    for (auto sym : rule_src) {
      //cerr << TD::Convert(sym & cdec::ALL_MASK) << endl;
      cur = &cur->next[sym];
      if (cdec::IsFrontier(sym)) {  // frontier symbols -> variables
        int nt = (sym & cdec::ALL_MASK);
        frhs.push_back(-nt);
      } else if (cdec::IsTerminal(sym)) {
        frhs.push_back(sym);
      } // else internal NT, nothing to do
    }
    os << '[' << TD::Convert(-lhs) << "] |||";
    for (auto x : frhs) {
      os << ' ';
      if (x < 0)
        os << '[' << TD::Convert(-x) << ']';
      else
        os << TD::Convert(x);
    }
    TRulePtr rule;
    if (has_multiple_states) {
      cerr << "Not implemented...\n"; abort(); // TODO read in states
    } else {
      os << " ||| " << fields[1] << " ||| " << fields[2];
      rule.reset(new TRule(os.str()));
    }
    cur->rules.push_back(rule);
    //cerr << "RULE: " << rule->AsString() << "\n\n";
  }
}

// represents where in an input parse tree the transducer must continue
// and what state it is in
struct TransducerState {
  TransducerState() : input_node_idx(), transducer_state() {}
  TransducerState(unsigned n, unsigned q) : input_node_idx(n), transducer_state(q) {}
  bool operator==(const TransducerState& o) const {
    return input_node_idx == o.input_node_idx &&
           transducer_state == o.transducer_state;
  }
  unsigned input_node_idx;
  unsigned transducer_state;
};

// represents the state of the composition algorithm
struct ParserState {
  ParserState() : in_iter(), node() {}
  cdec::TreeFragment::iterator in_iter;
  ParserState(const cdec::TreeFragment::iterator& it, unsigned q, Tree2StringGrammarNode* n) :
      in_iter(it),
      task(it.node_idx(), q),
      node(n) {}
  ParserState(const cdec::TreeFragment::iterator& it, Tree2StringGrammarNode* n, const ParserState& p) :
      in_iter(it),
      future_work(p.future_work),
      task(p.task),
      node(n) {}
  bool operator==(const ParserState& o) const {
    return node == o.node && task == o.task &&
           future_work == o.future_work && in_iter == o.in_iter;
  }
  vector<TransducerState> future_work;
  TransducerState task; // subtree root where and in what state did the transducer start?
  Tree2StringGrammarNode* node; // pointer into grammar trie
};

namespace std {
  template<>
  struct hash<TransducerState> {
    size_t operator()(const TransducerState& q) const {
      size_t h = boost::hash_value(q.transducer_state);
      boost::hash_combine(h, boost::hash_value(q.input_node_idx));
      return h;
    }
  };
  template<>
  struct hash<ParserState> {
    size_t operator()(const ParserState& s) const {
      size_t h = boost::hash_value(s.node);
      for (auto& w : s.future_work)
        boost::hash_combine(h, hash<TransducerState>()(w));
      boost::hash_combine(h, hash<TransducerState>()(s.task));
      // TODO hash with iterator
      return h;
    }
  }; 
};

void AddDummyGoalNode(Hypergraph* hg) {
  static const int kGOAL = -TD::Convert("Goal");
  unsigned old_goal_node_idx = hg->nodes_.size() - 1;
  int old_goal_cat = hg->nodes_[old_goal_node_idx].cat_;
  TRulePtr goal_rule(new TRule("[Goal] ||| [X] ||| [1]"));
  goal_rule->f_[0] = old_goal_cat;
  HG::Node* goal_node = hg->AddNode(kGOAL);
  goal_node->node_hash = 1;
  TailNodeVector tail(1, old_goal_node_idx);
  HG::Edge* new_edge = hg->AddEdge(goal_rule, tail);
  hg->ConnectEdgeToHeadNode(new_edge, goal_node);
}

struct Tree2StringTranslatorImpl {
  vector<boost::shared_ptr<Tree2StringGrammarNode>> root;
  bool add_pass_through_rules;
  bool has_multiple_states;
  unsigned remove_grammars;
  Tree2StringTranslatorImpl(const boost::program_options::variables_map& conf,
                            bool has_multiple_states) :
      add_pass_through_rules(conf.count("add_pass_through_rules")),
      has_multiple_states(has_multiple_states) {
    if (conf.count("grammar")) {
      const vector<string> gf = conf["grammar"].as<vector<string>>();
      root.resize(gf.size());
      unsigned gc = 0;
      for (auto& f : gf) {
        ReadFile rf(f);
        root[gc].reset(new Tree2StringGrammarNode);
        ReadTree2StringGrammar(rf.stream(), &*root[gc++], has_multiple_states);
      }
    }
  }

  // loads a per-sentence grammar
  void LoadSupplementalGrammar(const string& gfile) {
    root.resize(root.size() + 1);
    root.back().reset(new Tree2StringGrammarNode);
    ++remove_grammars;
    ReadFile rf(gfile);
    ReadTree2StringGrammar(rf.stream(), root.back().get(), has_multiple_states);
  }

  // src must be fully abstract
  bool DoesAbstractPassThroughRuleExist(unsigned state, const cdec::TreeFragment& src) const {
    unsigned len = root.size();
    if (len <= 1) return false;
    --len;
    for (unsigned i = 0; i < len; ++i) {
      const Tree2StringGrammarNode* cur = &*root[i];
      auto it = cur->next.find(state);
      if (it == cur->next.end()) continue;
      cur = &it->second;
      bool failed = false;
      vector<int> trg;
      for (auto sym : src) {
        it = cur->next.find(sym);
        if (it == cur->next.end()) { failed = true; break; }
        if (cdec::IsFrontier(sym)) trg.push_back(-trg.size());
        cur = &it->second;
      }
      if (failed) continue;
      // TODO check for destination states in t2t
      for (auto r : cur->rules)
        if (r->e_ == trg) return true;
    }
    return false;
  }

  void CreatePassThroughRules(const cdec::TreeFragment& tree) {
    static const int kFIDlex = FD::Convert("PassThrough_Lexical");
    static const int kFIDabs = FD::Convert("PassThrough_Abstract");
    static const int kFIDmix = FD::Convert("PassThrough_Mix");
    static const int kFID = FD::Convert("PassThrough");
    static unordered_map<int, int> pntfid;
    root.resize(root.size() + 1);
    root.back().reset(new Tree2StringGrammarNode);
    ++remove_grammars;
    unordered_set<vector<int>,boost::hash<vector<int>>> unique_rule_check;
    for (auto& prod : tree.nodes) {
      int ntc = 0;
      int lhs = -(prod.lhs & cdec::ALL_MASK);
      int &ntfid = pntfid[lhs];
      if (!ntfid) {
        ostringstream fos;
        fos << "PassThrough:" << TD::Convert(-lhs);
        ntfid = FD::Convert(fos.str());
      }

      // check for duplicate rule in tree
      vector<int> key;
      key.push_back(prod.lhs);

      bool has_lex = false;
      bool has_nt = false;
      vector<int> rhse, rhsf;
      ostringstream os;
      os << '(' << TD::Convert(-lhs);
      for (auto& sym : prod.rhs) {
        os << ' ';
        if (cdec::IsTerminal(sym)) {
          has_lex = true;
          os << TD::Convert(sym);
          rhse.push_back(sym);
          rhsf.push_back(sym);
          key.push_back(sym);
        } else {
          has_nt = true;
          unsigned id = tree.nodes[sym & cdec::ALL_MASK].lhs & cdec::ALL_MASK;
          os << '[' << TD::Convert(id) << ']';
          rhsf.push_back(-id);
          rhse.push_back(-ntc);
          key.push_back(-id);
          ++ntc;
        }
      }
      os << ')';
      if (!unique_rule_check.insert(key).second) continue;
      cdec::TreeFragment rule_src(os.str(), true);
      Tree2StringGrammarNode* cur = root.back().get();
      // do we need all transducer states here??? a list??? no pass through rules???
      unsigned transducer_state = 0;
      const bool abstract_rule = (has_nt && !has_lex);
      // the following reduces ambiguity quite a lot
      if (abstract_rule && DoesAbstractPassThroughRuleExist(transducer_state, rule_src)) continue; 
      cur = &cur->next[transducer_state];
      for (auto sym : rule_src)
        cur = &cur->next[sym];
      TRulePtr rule(new TRule(rhse, rhsf, lhs));
      rule->ComputeArity();
      rule->scores_.set_value(ntfid, 1.0);
      rule->scores_.set_value(kFID, 1.0);
      if (has_lex && has_nt)
        rule->scores_.set_value(kFIDmix, 1.0);
      else if (has_lex) rule->scores_.set_value(kFIDlex, 1.0);
      else if (has_nt) rule->scores_.set_value(kFIDabs, 1.0);
      cur->rules.push_back(rule);
    }
  }

  void RemoveGrammars() {
    assert(remove_grammars <= root.size());
    root.resize(root.size() - remove_grammars);
  }

  bool Translate(const string& input,
                 SentenceMetadata* smeta,
                 const vector<double>& weights,
                 Hypergraph* minus_lm_forest) {
    cdec::TreeFragment input_tree(input, false);
    if (add_pass_through_rules) CreatePassThroughRules(input_tree);
    Hypergraph hg;
    hg.ReserveNodes(input_tree.nodes.size());
    unordered_map<TransducerState, unsigned> x2hg(input_tree.nodes.size() * 5);
    queue<ParserState> q;
    unordered_set<ParserState> unique;  // only create items one time
    for (auto& g : root) {
      unsigned q_0 = 0; // TODO initialize q_0 properly once multi-state transducers are supported
      auto rit = g->next.find(q_0);
      if (rit != g->next.end()) { // does this g have this transducer state?
        q.push(ParserState(input_tree.begin(), q_0, &rit->second));
        unique.insert(q.back());
      }
    }
    if (q.size() == 0) return false;
    const TransducerState tree_top = q.front().task;
    while(!q.empty()) {
      ParserState& s = q.front();

      if (s.in_iter.at_end()) { // completed a traversal of a subtree
        //cerr << "I traversed a subtree of the input rooted at node=" << s.input_node_idx << " sym=" << 
        //   TD::Convert(input_tree.nodes[s.input_node_idx].lhs & cdec::ALL_MASK) << endl;
        if (s.node->rules.size()) {
          auto it = x2hg.find(s.task);
          if (it == x2hg.end()) {
            // TODO create composite state symbol that encodes transducer state type?
            HG::Node* new_node = hg.AddNode(-(input_tree.nodes[s.task.input_node_idx].lhs & cdec::ALL_MASK));
            new_node->node_hash = std::hash<TransducerState>()(s.task);
            it = x2hg.insert(make_pair(s.task, new_node->id_)).first;
          }
          const unsigned node_id = it->second;
          TailNodeVector tail;
          for (const auto& n : s.future_work) {
            auto it = x2hg.find(n);
            if (it == x2hg.end()) {
              // TODO create composite state symbol that encodes transducer state type?
              HG::Node* new_node = hg.AddNode(-(input_tree.nodes[n.input_node_idx].lhs & cdec::ALL_MASK));
              new_node->node_hash = std::hash<TransducerState>()(n);
              it = x2hg.insert(make_pair(n, new_node->id_)).first;
            }
            tail.push_back(it->second);
          }
          for (auto& r : s.node->rules) {
            assert(tail.size() == r->Arity());
            HG::Edge* new_edge = hg.AddEdge(r, tail);
            new_edge->feature_values_ = r->GetFeatureValues();
            auto& inspan = input_tree.nodes[s.task.input_node_idx].span;
            new_edge->i_ = inspan.first;
            new_edge->j_ = inspan.second;
            hg.ConnectEdgeToHeadNode(new_edge, &hg.nodes_[node_id]);
          }
          for (const auto& n : s.future_work) {
            const auto it = input_tree.begin(n.input_node_idx); // start tree iterator at node n
            for (auto& g : root) {
              auto rit = g->next.find(n.transducer_state);
              if (rit != g->next.end()) { // does this g have this transducer state?
                const ParserState s(it, n.transducer_state, &rit->second);
                if (unique.insert(s).second) q.push(s);
              }
            }
          }
        } else {
          //cerr << "I can't build anything :(\n";
        }
      } else { // more input tree to match
        unsigned sym = *s.in_iter;
        if (cdec::IsLHS(sym)) {
          auto nit = s.node->next.find(sym);
          if (nit != s.node->next.end()) {
            //cerr << "MATCHED LHS: " << TD::Convert(sym & cdec::ALL_MASK) << endl;
            ParserState news(++s.in_iter, &nit->second, s);
            if (unique.insert(news).second) q.push(news);
          }
        } else if (cdec::IsRHS(sym)) {
          //cerr << "Attempting to match RHS: " << TD::Convert(sym & cdec::ALL_MASK) << endl;
          cdec::TreeFragment::iterator var = s.in_iter;
          var.truncate();
          auto nit1 = s.node->next.find(sym);
          auto nit2 = s.node->next.find(*var);
          if (nit2 != s.node->next.end()) {
            //cerr << "MATCHED VAR RHS: " << TD::Convert(sym & cdec::ALL_MASK) << endl;
            ++var;
            // TODO: find out from rule what the new target state is (the 0 in the next line)
            // if it is associated with the rule, we won't know until we match the whole input
            // so the 0 may be okay (if this is the case, which is probably the easiest thing,
            // then the state must be dealt with when the future work becomes real work)
            const TransducerState new_task(s.in_iter.child_node(), 0);
            ParserState new_s(var, &nit2->second, s);
            new_s.future_work.push_back(new_task);  // if this traversal of the input succeeds, future_work goes on the q
            if (unique.insert(new_s).second) q.push(new_s);
          }
          //else { cerr << "did not match [" << TD::Convert(sym & cdec::ALL_MASK) << "]\n"; }
          if (nit1 != s.node->next.end()) {
            //cerr << "MATCHED FULL RHS: " << TD::Convert(sym & cdec::ALL_MASK) << endl;
            const ParserState new_s(++s.in_iter, &nit1->second, s);
            if (unique.insert(new_s).second) q.push(new_s);
          }
          //else { cerr << "did not match " << TD::Convert(sym & cdec::ALL_MASK) << "\n"; }
        } else if (cdec::IsTerminal(sym)) {
          auto nit = s.node->next.find(sym);
          if (nit != s.node->next.end()) {
            //cerr << "MATCHED TERMINAL: " << TD::Convert(sym) << endl;
            const ParserState new_s(++s.in_iter, &nit->second, s);
            if (unique.insert(new_s).second) q.push(new_s);
          }
        } else {
          cerr << "This can never happen!\n"; abort();
        }
      }
      q.pop();
    }
    const auto goal_it = x2hg.find(tree_top);
    if (goal_it == x2hg.end()) return false;
    //cerr << "Goal node: " << goal << endl;
    hg.TopologicallySortNodesAndEdges(goal_it->second);

    // there might be nodes that cannot be derived
    // the following takes care of them
    vector<bool> prune(hg.edges_.size(), false);
    hg.PruneEdges(prune, true);
    if (hg.edges_.size() == 0) return false;
    // rescoring assumes the goal edge is arity 1 (code laziness), add that here
    AddDummyGoalNode(&hg);

    hg.Reweight(weights);
    //hg.PrintGraphviz();

    minus_lm_forest->swap(hg);
    return true;
  }
};

Tree2StringTranslator::Tree2StringTranslator(const boost::program_options::variables_map& conf,
                                             bool has_multiple_states) :
  pimpl_(new Tree2StringTranslatorImpl(conf, has_multiple_states)) {}

void Tree2StringTranslator::ProcessMarkupHintsImpl(const map<string, string>& kv) {
  pimpl_->remove_grammars = 0;
  if (kv.find("grammar0") != kv.end()) {
    cerr << "SGML tag grammar0 is not expected (order is: grammar, grammar1, grammar2, ...)\n";
    abort();
  }
  unsigned gc = 0;
  set<string> loaded;
  while(true) {
    string gkey = "grammar";
    if (gc > 0) gkey += boost::lexical_cast<string>(gc);
    ++gc;
    map<string,string>::const_iterator it = kv.find(gkey);
    if (it == kv.end()) break;
    const string& gfile = it->second;
    if (loaded.count(gfile) == 1) {
      cerr << "Attempting to load " << gfile << " twice!\n";
      abort();
    }
    loaded.insert(gfile);
    pimpl_->LoadSupplementalGrammar(gfile);
  }
}

bool Tree2StringTranslator::TranslateImpl(const string& input,
                               SentenceMetadata* smeta,
                               const vector<double>& weights,
                               Hypergraph* minus_lm_forest) {
  return pimpl_->Translate(input, smeta, weights, minus_lm_forest);
}

void Tree2StringTranslator::SentenceCompleteImpl() {
  pimpl_->RemoveGrammars();
}

std::string Tree2StringTranslator::GetDecoderType() const {
  return "tree2string";
}

