#include <algorithm>
#include <vector>
#include <queue>
#include <boost/functional/hash.hpp>
#include <unordered_map>
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

void ReadTree2StringGrammar(istream* in, Tree2StringGrammarNode* root) {
  string line;
  while(getline(*in, line)) {
    size_t pos = line.find("|||");
    assert(pos != string::npos);
    assert(pos > 3);
    unsigned xc = 0;
    while (line[pos - 1] == ' ') { --pos; xc++; }
    cdec::TreeFragment rule_src(line.substr(0, pos), true);
    Tree2StringGrammarNode* cur = root;
    ostringstream os;
    int lhs = -(rule_src.root & cdec::ALL_MASK);
    // build source RHS for SCFG projection
    // TODO - this is buggy - it will generate a well-formed SCFG rule
    // but it will not generate source strings correctly
    vector<int> frhs;
    for (auto sym : rule_src) {
      cur = &cur->next[sym];
      if (sym) {
        if (cdec::IsFrontier(sym)) {  // frontier symbols -> variables
          int nt = (sym & cdec::ALL_MASK);
          frhs.push_back(-nt);
        } else if (cdec::IsTerminal(sym)) {
          frhs.push_back(sym);
        }
      }
    }
    os << '[' << TD::Convert(-lhs) << "] |||";
    for (auto x : frhs) {
      os << ' ';
      if (x < 0)
        os << '[' << TD::Convert(-x) << ']';
      else
        os << TD::Convert(x);
    }
    pos += 3 + xc;
    while(line[pos] == ' ') { ++pos; }
    os << " ||| " << line.substr(pos);
    TRulePtr rule(new TRule(os.str()));
    cur->rules.push_back(rule);
  }
}

struct ParserState {
  ParserState() : in_iter(), node() {}
  cdec::TreeFragment::iterator in_iter;
  ParserState(const cdec::TreeFragment::iterator& it, Tree2StringGrammarNode* n) :
      in_iter(it),
      input_node_idx(it.node_idx()),
      node(n) {}
  ParserState(const cdec::TreeFragment::iterator& it, Tree2StringGrammarNode* n, const ParserState& p) :
      in_iter(it),
      future_work(p.future_work),
      input_node_idx(p.input_node_idx),
      node(n) {}
  vector<ParserState> future_work;
  int input_node_idx; // lhs of top level NT
  Tree2StringGrammarNode* node;
};

struct Tree2StringTranslatorImpl {
  Tree2StringGrammarNode root;
  Tree2StringTranslatorImpl(const boost::program_options::variables_map& conf) {
    ReadFile rf(conf["grammar"].as<vector<string>>()[0]);
    ReadTree2StringGrammar(rf.stream(), &root);
  }
  bool Translate(const string& input,
                 SentenceMetadata* smeta,
                 const vector<double>& weights,
                 Hypergraph* minus_lm_forest) {
    cdec::TreeFragment input_tree(input, false);
    Hypergraph hg;
    hg.ReserveNodes(input_tree.nodes.size());
    vector<int> tree2hg(input_tree.nodes.size() + 1, -1);
    queue<ParserState> q;
    q.push(ParserState(input_tree.begin(), &root));
    unsigned tree_top = q.front().input_node_idx;
    while(!q.empty()) {
      ParserState& s = q.front();

      if (s.in_iter.at_end()) { // completed a traversal of a subtree
        //cerr << "I traversed a subtree of the input rooted at node=" << s.input_node_idx << " sym=" << 
        //   TD::Convert(input_tree.nodes[s.input_node_idx].lhs & cdec::ALL_MASK) << endl;
        if (s.node->rules.size()) {
          TailNodeVector tail;
          int& node_id = tree2hg[s.input_node_idx];
          if (node_id < 0)
            node_id = hg.AddNode(-(input_tree.nodes[s.input_node_idx].lhs & cdec::ALL_MASK))->id_;
          for (auto& n : s.future_work) {
            int& nix = tree2hg[n.input_node_idx];
            if (nix < 0)
              nix = hg.AddNode(-(input_tree.nodes[n.input_node_idx].lhs & cdec::ALL_MASK))->id_;
            tail.push_back(nix);
          }
          for (auto& r : s.node->rules) {
            assert(tail.size() == r->Arity());
            HG::Edge* new_edge = hg.AddEdge(r, tail);
            new_edge->feature_values_ = r->GetFeatureValues();
            // TODO: set i and j
            hg.ConnectEdgeToHeadNode(new_edge, &hg.nodes_[node_id]);
          }
          for (auto& w : s.future_work)
            q.push(w);
        } else {
          //cerr << "I can't build anything :(\n";
        }
      } else { // more input tree to match
        unsigned sym = *s.in_iter;
        if (cdec::IsLHS(sym)) {
          auto nit = s.node->next.find(sym);
          if (nit != s.node->next.end()) {
            //cerr << "MATCHED LHS: " << TD::Convert(sym & cdec::ALL_MASK) << endl;
            q.push(ParserState(++s.in_iter, &nit->second, s));
          }
        } else if (cdec::IsRHS(sym)) {
          //cerr << "Attempting to match RHS: " << TD::Convert(sym & cdec::ALL_MASK) << endl;
          cdec::TreeFragment::iterator var = s.in_iter;
          var.truncate();
          auto nit1 = s.node->next.find(sym);
          auto nit2 = s.node->next.find(*var);
          if (nit2 != s.node->next.end()) {
            //cerr << "MATCHED VAR RHS: " << TD::Convert(sym & cdec::ALL_MASK) << endl;
            ParserState new_s(++var, &nit2->second, s);
            ParserState new_work(s.in_iter.remainder(), &root);
            new_s.future_work.push_back(new_work);  // if this traversal of the input succeeds, future_work goes on the q
            q.push(new_s);
          }
          if (nit1 != s.node->next.end()) {
            //cerr << "MATCHED FULL RHS: " << TD::Convert(sym & cdec::ALL_MASK) << endl;
            q.push(ParserState(++s.in_iter, &nit1->second, s));
          }
        } else if (cdec::IsTerminal(sym)) {
          auto nit = s.node->next.find(sym);
          if (nit != s.node->next.end()) {
            //cerr << "MATCHED TERMINAL: " << TD::Convert(sym) << endl;
            q.push(ParserState(++s.in_iter, &nit->second, s));
          }
        } else {
          cerr << "This can never happen!\n"; abort();
        }
      }
      q.pop();
    }
    int goal = tree2hg[tree_top];
    if (goal < 0) return false;
    //cerr << "Goal node: " << goal << endl;
    hg.TopologicallySortNodesAndEdges(goal);
    hg.Reweight(weights);
    //hg.PrintGraphviz();
    minus_lm_forest->swap(hg);
    return true;
  }
};

Tree2StringTranslator::Tree2StringTranslator(const boost::program_options::variables_map& conf) :
  pimpl_(new Tree2StringTranslatorImpl(conf)) {}

bool Tree2StringTranslator::TranslateImpl(const string& input,
                               SentenceMetadata* smeta,
                               const vector<double>& weights,
                               Hypergraph* minus_lm_forest) {
  return pimpl_->Translate(input, smeta, weights, minus_lm_forest);
}

void Tree2StringTranslator::ProcessMarkupHintsImpl(const map<string, string>& kv) {
}

void Tree2StringTranslator::SentenceCompleteImpl() {
}

std::string Tree2StringTranslator::GetDecoderType() const {
  return "tree2string";
}

