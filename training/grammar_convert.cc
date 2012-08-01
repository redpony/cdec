/*
  this program modifies cfg hypergraphs (forests) and extracts kbests?
  what are: json, split ?
 */
#include <iostream>
#include <algorithm>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "inside_outside.h"
#include "tdict.h"
#include "filelib.h"
#include "hg.h"
#include "hg_io.h"
#include "kbest.h"
#include "viterbi.h"
#include "weights.h"

namespace po = boost::program_options;
using namespace std;

WordID kSTART;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input,i", po::value<string>()->default_value("-"), "Input file")
        ("format,f", po::value<string>()->default_value("cfg"), "Input format. Values: cfg, json, split")
        ("output,o", po::value<string>()->default_value("json"), "Output command. Values: json, 1best")
        ("reorder,r", "Add Yamada & Knight (2002) reorderings")
        ("weights,w", po::value<string>(), "Feature weights for k-best derivations [optional]")
        ("collapse_weights,C", "Collapse order features into a single feature whose value is all of the locally applying feature weights")
        ("k_derivations,k", po::value<int>(), "Show k derivations and their features")
        ("max_reorder,m", po::value<int>()->default_value(999), "Move a constituent at most this far")
        ("help,h", "Print this help message and exit");
  po::options_description clo("Command line options");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);

  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  po::notify(*conf);

  if (conf->count("help") || conf->count("input") == 0) {
    cerr << "\nUsage: grammar_convert [-options]\n\nConverts a grammar file (in Hiero format) into JSON hypergraph.\n";
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

int GetOrCreateNode(const WordID& lhs, map<WordID, int>* lhs2node, Hypergraph* hg) {
  int& node_id = (*lhs2node)[lhs];
  if (!node_id)
    node_id = hg->AddNode(lhs)->id_ + 1;
  return node_id - 1;
}

void FilterAndCheckCorrectness(int goal, Hypergraph* hg) {
  if (goal < 0) {
    cerr << "Error! [S] not found in grammar!\n";
    exit(1);
  }
  if (hg->nodes_[goal].in_edges_.size() != 1) {
    cerr << "Error! [S] has more than one rewrite!\n";
    exit(1);
  }
  int old_size = hg->nodes_.size();
  hg->TopologicallySortNodesAndEdges(goal);
  if (hg->nodes_.size() != old_size) {
    cerr << "Warning! During sorting " << (old_size - hg->nodes_.size()) << " disappeared!\n";
  }
  vector<double> inside; // inside score at each node
  double p = Inside<double, TransitionCountWeightFunction>(*hg, &inside);
  if (!p) {
    cerr << "Warning! Grammar defines the empty language!\n";
    hg->clear();
    return;
  }
  vector<bool> prune(hg->edges_.size(), false);
  int bad_edges = 0;
  for (unsigned i = 0; i < hg->edges_.size(); ++i) {
    Hypergraph::Edge& edge = hg->edges_[i];
    bool bad = false;
    for (unsigned j = 0; j < edge.tail_nodes_.size(); ++j) {
      if (!inside[edge.tail_nodes_[j]]) {
        bad = true;
        ++bad_edges;
      }
    }
    prune[i] = bad;
  }
  cerr << "Removing " << bad_edges << " bad edges from the grammar.\n";
  for (unsigned i = 0; i < hg->edges_.size(); ++i) {
    if (prune[i])
      cerr << "   " << hg->edges_[i].rule_->AsString() << endl;
  }
  hg->PruneEdges(prune);
}

void CreateEdge(const TRulePtr& r, const Hypergraph::TailNodeVector& tail, Hypergraph::Node* head_node, Hypergraph* hg) {
  Hypergraph::Edge* new_edge = hg->AddEdge(r, tail);
  hg->ConnectEdgeToHeadNode(new_edge, head_node);
  new_edge->feature_values_ = r->scores_;
}

// from a category label like "NP_2", return "NP"
string PureCategory(WordID cat) {
  assert(cat < 0);
  string c = TD::Convert(cat*-1);
  size_t p = c.find("_");
  if (p == string::npos) return c;
  return c.substr(0, p);
};

string ConstituentOrderFeature(const TRule& rule, const vector<int>& pi) {
  const static string kTERM_VAR = "x";
  const vector<WordID>& f = rule.f();
  map<string, int> used;
  vector<string> terms(f.size());
  for (int i = 0; i < f.size(); ++i) {
    const string term = (f[i] < 0 ? PureCategory(f[i]) : kTERM_VAR);
    int& count = used[term];
    if (!count) {
      terms[i] = term;
    } else {
      ostringstream os;
      os << term << count;
      terms[i] = os.str();
    }
    ++count;
  }
  ostringstream os;
  os << PureCategory(rule.GetLHS()) << ':';
  for (int i = 0; i < f.size(); ++i) {
    if (i > 0) os << '_';
    os << terms[pi[i]];
  }
  return os.str();
}

bool CheckPermutationMask(const vector<int>& mask, const vector<int>& pi) {
  assert(mask.size() == pi.size());

  int req_min = -1;
  int cur_max = 0;
  int cur_mask = -1;
  for (int i = 0; i < mask.size(); ++i) {
    if (mask[i] != cur_mask) {
      cur_mask = mask[i];
      req_min = cur_max - 1;
    }
    if (pi[i] > req_min) {
      if (pi[i] > cur_max) cur_max = pi[i];
    } else {
      return false;
    }
  }

  return true;
}

void PermuteYKRecursive(int nodeid, const WordID& parent, const int max_reorder, Hypergraph* hg) {
  // Hypergraph tmp = *hg;
  Hypergraph::Node* node = &hg->nodes_[nodeid];
  if (node->in_edges_.size() != 1) {
    cerr << "Multiple rewrites of [" << TD::Convert(node->cat_ * -1) << "] (parent is [" << TD::Convert(parent*-1) << "])\n";
    cerr << "  not recursing!\n";
    return;
  }
//  for (int eii = 0; eii < node->in_edges_.size(); ++eii) {
    const int oe_index = node->in_edges_.front();
    const TRule& rule = *hg->edges_[oe_index].rule_;
    const Hypergraph::TailNodeVector orig_tail = hg->edges_[oe_index].tail_nodes_;
    const int tail_size = orig_tail.size();
    for (int i = 0; i < tail_size; ++i) {
      PermuteYKRecursive(hg->edges_[oe_index].tail_nodes_[i], node->cat_, max_reorder, hg);
    }
    const vector<WordID>& of = rule.f_;
    if (of.size() == 1) return;
  //  cerr << "Permuting [" << TD::Convert(node->cat_ * -1) << "]\n";
  //  cerr << "ORIG: " << rule.AsString() << endl;
    vector<WordID> pi(of.size(), 0);
    for (int i = 0; i < pi.size(); ++i) pi[i] = i;

    vector<int> permutation_mask(of.size(), 0);
    const bool dont_reorder_across_PU = true;  // TODO add configuration
    if (dont_reorder_across_PU) {
      int cur = 0;
      for (int i = 0; i < pi.size(); ++i) {
        if (of[i] >= 0) continue;
        const string cat = PureCategory(of[i]);
        if (cat == "PU" || cat == "PU!H" || cat == "PUNC" || cat == "PUNC!H" || cat == "CC") {
          ++cur;
          permutation_mask[i] = cur;
          ++cur;
        } else {
          permutation_mask[i] = cur;
        }
      }
    }
    int fid = FD::Convert(ConstituentOrderFeature(rule, pi));
    hg->edges_[oe_index].feature_values_.set_value(fid, 1.0);
    while (next_permutation(pi.begin(), pi.end())) {
      if (!CheckPermutationMask(permutation_mask, pi))
        continue;
      vector<WordID> nf(pi.size(), 0);
      Hypergraph::TailNodeVector tail(pi.size(), 0);
      bool skip = false;
      for (int i = 0; i < pi.size(); ++i) {
        int dist = pi[i] - i; if (dist < 0) dist *= -1;
        if (dist > max_reorder) { skip = true; break; }
        nf[i] = of[pi[i]];
        tail[i] = orig_tail[pi[i]];
      }
      if (skip) continue;
      TRulePtr nr(new TRule(rule));
      nr->f_ = nf;
      int fid = FD::Convert(ConstituentOrderFeature(rule, pi));
      nr->scores_.set_value(fid, 1.0);
  //    cerr << "PERM: " << nr->AsString() << endl;
      CreateEdge(nr, tail, node, hg);
    }
 // }
}

void PermuteYamadaAndKnight(Hypergraph* hg, int max_reorder) {
  assert(hg->nodes_.back().cat_ == kSTART);
  assert(hg->nodes_.back().in_edges_.size() == 1);
  PermuteYKRecursive(hg->nodes_.size() - 1, kSTART, max_reorder, hg);
}

void CollapseWeights(Hypergraph* hg) {
  int fid = FD::Convert("Reordering");
  for (int i = 0; i < hg->edges_.size(); ++i) {
    Hypergraph::Edge& edge = hg->edges_[i];
    edge.feature_values_.clear();
    if (edge.edge_prob_ != prob_t::Zero()) {
      edge.feature_values_.set_value(fid, log(edge.edge_prob_));
    }
  }
}

void ProcessHypergraph(const vector<double>& w, const po::variables_map& conf, const string& ref, Hypergraph* hg) {
  if (conf.count("reorder"))
    PermuteYamadaAndKnight(hg, conf["max_reorder"].as<int>());
  if (w.size() > 0) { hg->Reweight(w); }
  if (conf.count("collapse_weights")) CollapseWeights(hg);
  if (conf["output"].as<string>() == "json") {
    HypergraphIO::WriteToJSON(*hg, false, &cout);
    if (!ref.empty()) { cerr << "REF: " << ref << endl; }
  } else {
    vector<WordID> onebest;
    ViterbiESentence(*hg, &onebest);
    if (ref.empty()) {
      cout << TD::GetString(onebest) << endl;
    } else {
      cout << TD::GetString(onebest) << " ||| " << ref << endl;
    }
  }
  if (conf.count("k_derivations")) {
    const int k = conf["k_derivations"].as<int>();
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(*hg, k);
    for (int i = 0; i < k; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
        kbest.LazyKthBest(hg->nodes_.size() - 1, i);
      if (!d) break;
      cerr << log(d->score) << " ||| " << TD::GetString(d->yield) << " ||| " << d->feature_values << endl;
    }
  }
}

int main(int argc, char **argv) {
  kSTART = TD::Convert("S") * -1;
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  string infile = conf["input"].as<string>();
  const bool is_split_input = (conf["format"].as<string>() == "split");
  const bool is_json_input = is_split_input || (conf["format"].as<string>() == "json");
  const bool collapse_weights = conf.count("collapse_weights");
  vector<double> w;
  if (conf.count("weights"))
    Weights::InitFromFile(conf["weights"].as<string>(), &w);

  if (collapse_weights && !w.size()) {
    cerr << "--collapse_weights requires a weights file to be specified!\n";
    exit(1);
  }
  ReadFile rf(infile);
  istream* in = rf.stream();
  assert(*in);
  int lc = 0;
  Hypergraph hg;
  map<WordID, int> lhs2node;
  while(*in) {
    string line;
    ++lc;
    getline(*in, line);
    if (is_json_input) {
      if (line.empty() || line[0] == '#') continue;
      string ref;
      if (is_split_input) {
        size_t pos = line.rfind("}}");
        assert(pos != string::npos);
        size_t rstart = line.find("||| ", pos);
        assert(rstart != string::npos);
        ref = line.substr(rstart + 4);
        line = line.substr(0, pos + 2);
      }
      istringstream is(line);
      if (HypergraphIO::ReadFromJSON(&is, &hg)) {
        ProcessHypergraph(w, conf, ref, &hg);
        hg.clear();
      } else {
        cerr << "Error reading grammar from JSON: line " << lc << endl;
        exit(1);
      }
    } else {
      if (line.empty()) {
        int goal = lhs2node[kSTART] - 1;
        FilterAndCheckCorrectness(goal, &hg);
        ProcessHypergraph(w, conf, "", &hg);
        hg.clear();
        lhs2node.clear();
        continue;
      }
      if (line[0] == '#') continue;
      if (line[0] != '[') {
        cerr << "Line " << lc << ": bad format\n";
        exit(1);
      }
      TRulePtr tr(TRule::CreateRuleMonolingual(line));
      Hypergraph::TailNodeVector tail;
      for (int i = 0; i < tr->f_.size(); ++i) {
        WordID var_cat = tr->f_[i];
        if (var_cat < 0)
          tail.push_back(GetOrCreateNode(var_cat, &lhs2node, &hg));
      }
      const WordID lhs = tr->GetLHS();
      int head = GetOrCreateNode(lhs, &lhs2node, &hg);
      Hypergraph::Edge* edge = hg.AddEdge(tr, tail);
      edge->feature_values_ = tr->scores_;
      Hypergraph::Node* node = &hg.nodes_[head];
      hg.ConnectEdgeToHeadNode(edge, node);
    }
  }
}

