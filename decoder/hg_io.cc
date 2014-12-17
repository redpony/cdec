#include "hg_io.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/shared_ptr.hpp>

#include "fast_lexical_cast.hpp"

#include "tdict.h"
#include "hg.h"

using namespace std;

bool HypergraphIO::ReadFromBinary(istream* in, Hypergraph* hg) {
  boost::archive::binary_iarchive oa(*in);
  hg->clear();
  oa >> *hg;
  return true;
}

bool HypergraphIO::WriteToBinary(const Hypergraph& hg, ostream* out) {
  boost::archive::binary_oarchive oa(*out);
  oa << hg;
  return true;
}

bool needs_escape[128];
void InitEscapes() {
  memset(needs_escape, false, 128);
  needs_escape[static_cast<size_t>('\'')] = true;
  needs_escape[static_cast<size_t>('\\')] = true;
}

string HypergraphIO::Escape(const string& s) {
  size_t len = s.size();
  for (int i = 0; i < s.size(); ++i) {
    unsigned char c = s[i];
    if (c < 128 && needs_escape[c]) ++len;
  }
  if (len == s.size()) return s;
  string res(len, ' ');
  size_t o = 0;
  for (int i = 0; i < s.size(); ++i) {
    unsigned char c = s[i];
    if (c < 128 && needs_escape[c])
      res[o++] = '\\';
    res[o++] = c;
  }
  assert(o == len);
  return res;
}

string HypergraphIO::AsPLF(const Hypergraph& hg, bool include_global_parentheses) {
  static bool first = true;
  if (first) { InitEscapes(); first = false; }
  if (hg.nodes_.empty()) return "()";
  ostringstream os;
  if (include_global_parentheses) os << '(';
  static const string EPS="*EPS*";
  for (int i = 0; i < hg.nodes_.size()-1; ++i) {
    if (hg.nodes_[i].out_edges_.empty()) abort();
    const bool last_node = (i == hg.nodes_.size() - 2);
    const int out_edges_size = hg.nodes_[i].out_edges_.size();
    // compound splitter adds an extra goal transition which we suppress with
    // the following conditional
    if (!last_node || out_edges_size != 1 ||
         hg.edges_[hg.nodes_[i].out_edges_[0]].rule_->EWords() == 1) {
      os << '(';
      for (int j = 0; j < out_edges_size; ++j) {
        const Hypergraph::Edge& e = hg.edges_[hg.nodes_[i].out_edges_[j]];
        const string output = e.rule_->e_.size() ==2 ? Escape(TD::Convert(e.rule_->e_[1])) : EPS;
        double prob = log(e.edge_prob_);
        if (std::isinf(prob)) { prob = -9e20; }
        if (std::isnan(prob)) { prob = 0; }
        os << "('" << output << "'," << prob << "," << e.head_node_ - i << "),";
      }
      os << "),";
    }
  }
  if (include_global_parentheses) os << ')';
  return os.str();
}

string HypergraphIO::AsPLF(const Lattice& lat, bool include_global_parentheses) {
  static bool first = true;
  if (first) { InitEscapes(); first = false; }
  if (lat.empty()) return "()";
  ostringstream os;
  if (include_global_parentheses) os << '(';
  static const string EPS="*EPS*";
  for (int i = 0; i < lat.size(); ++i) {
    const vector<LatticeArc> arcs = lat[i];
    os << '(';
    for (int j = 0; j < arcs.size(); ++j) {
      os << "('" << Escape(TD::Convert(arcs[j].label)) << "',"
                 << arcs[j].cost << ',' << arcs[j].dist2next << "),";
    }
    os << "),";
  }
  if (include_global_parentheses) os << ')';
  return os.str();
}

namespace PLF {

const string chars = "'\\";
const char& quote = chars[0];
const char& slash = chars[1];

// safe get
inline char get(const std::string& in, int c) {
  if (c < 0 || c >= (int)in.size()) return 0;
  else return in[(size_t)c];
}

// consume whitespace
inline void eatws(const std::string& in, int& c) {
  while (get(in,c) == ' ') { c++; }
}

// from 'foo' return foo
std::string getEscapedString(const std::string& in, int &c)
{
  eatws(in,c);
  if (get(in,c++) != quote) return "ERROR";
  std::string res;
  char cur = 0;
  do {
    cur = get(in,c++);
    if (cur == slash) { res += get(in,c++); }
    else if (cur != quote) { res += cur; }
  } while (get(in,c) != quote && (c < (int)in.size()));
  c++;
  eatws(in,c);
  return res;
}

// basically atof
float getFloat(const std::string& in, int &c)
{
  std::string tmp;
  eatws(in,c);
  while (c < (int)in.size() && get(in,c) != ' ' && get(in,c) != ')' && get(in,c) != ',') {
    tmp += get(in,c++);
  }
  eatws(in,c);
  if (tmp.empty()) {
    cerr << "Syntax error while reading number! col=" << c << endl;
    abort();
  }
  return atof(tmp.c_str());
}

// basically atoi
int getInt(const std::string& in, int &c)
{
  std::string tmp;
  eatws(in,c);
  while (c < (int)in.size() && get(in,c) != ' ' && get(in,c) != ')' && get(in,c) != ',') {
    tmp += get(in,c++);
  }
  eatws(in,c);
  return atoi(tmp.c_str());
}

// maximum number of nodes permitted
#define MAX_NODES 100000000
// parse ('foo', 0.23)
void ReadPLFEdge(const std::string& in, int &c, int cur_node, Hypergraph* hg) {
  if (get(in,c++) != '(') { cerr << "PCN/PLF parse error: expected (\n"; abort(); }
  vector<WordID> ewords(2, 0);
  ewords[1] = TD::Convert(getEscapedString(in,c));
  TRulePtr r(new TRule(ewords));
  r->ComputeArity();
  // cerr << "RULE: " << r->AsString() << endl;
  if (get(in,c++) != ',') { cerr << in << endl; cerr << "PCN/PLF parse error: expected , after string\n"; abort(); }
  size_t cnNext = 1;
  std::vector<float> probs;
  probs.push_back(getFloat(in,c));
  while (get(in,c) == ',') {
    c++;
    float val = getFloat(in,c);
    probs.push_back(val);
    // cerr << val << endl;  //REMO
  }
  //if we read more than one prob, this was a lattice, last item was column increment
  if (probs.size()>1) {
    cnNext = static_cast<size_t>(probs.back());
    probs.pop_back();
    if (cnNext < 1) { cerr << cnNext << endl << "PCN/PLF parse error: bad link length at last element of cn alt block\n"; abort(); }
  }
  if (get(in,c++) != ')') { cerr << "PCN/PLF parse error: expected ) at end of cn alt block\n"; abort(); }
  eatws(in,c);
  Hypergraph::TailNodeVector tail(1, cur_node);
  Hypergraph::Edge* edge = hg->AddEdge(r, tail);
  //cerr << "  <--" << cur_node << endl;
  int head_node = cur_node + cnNext;
  assert(head_node < MAX_NODES);  // prevent malicious PLFs from using all the memory
  if (hg->nodes_.size() < (head_node + 1)) { hg->ResizeNodes(head_node + 1); }
  hg->ConnectEdgeToHeadNode(edge, &hg->nodes_[head_node]);
  for (int i = 0; i < probs.size(); ++i)
    edge->feature_values_.set_value(FD::Convert("Feature_" + boost::lexical_cast<string>(i)), probs[i]);
}

// parse (('foo', 0.23), ('bar', 0.77))
void ReadPLFNode(const std::string& in, int &c, int cur_node, int line, Hypergraph* hg) {
  //cerr << "PLF READING NODE " << cur_node << endl;
  if (hg->nodes_.size() < (cur_node + 1)) { hg->ResizeNodes(cur_node + 1); }
  if (get(in,c++) != '(') { cerr << line << ": Syntax error 1\n"; abort(); }
  eatws(in,c);
  while (1) {
    if (c > (int)in.size()) { break; }
    if (get(in,c) == ')') {
      c++;
      eatws(in,c);
      break;
    }
    if (get(in,c) == ',' && get(in,c+1) == ')') {
      c+=2;
      eatws(in,c);
      break;
    }
    if (get(in,c) == ',') { c++; eatws(in,c); }
    ReadPLFEdge(in, c, cur_node, hg);
  }
}

} // namespace PLF

void HypergraphIO::ReadFromPLF(const std::string& in, Hypergraph* hg, int line) {
  hg->clear();
  int c = 0;
  int cur_node = 0;
  if (in[c++] != '(') { cerr << line << ": Syntax error!\n"; abort(); }
  while (1) {
    if (c > (int)in.size()) { break; }
    if (PLF::get(in,c) == ')') {
      c++;
      PLF::eatws(in,c);
      break;
    }
    if (PLF::get(in,c) == ',' && PLF::get(in,c+1) == ')') {
      c+=2;
      PLF::eatws(in,c);
      break;
    }
    if (PLF::get(in,c) == ',') { c++; PLF::eatws(in,c); }
    PLF::ReadPLFNode(in, c, cur_node, line, hg);
    ++cur_node;
  }
  assert(cur_node == hg->nodes_.size() - 1);
}

void HypergraphIO::PLFtoLattice(const string& plf, Lattice* pl) {
  Lattice& l = *pl;
  Hypergraph g;
  ReadFromPLF(plf, &g, 0);
  const int num_nodes = g.nodes_.size() - 1;
  l.resize(num_nodes);
  int fid0=FD::Convert("Feature_0");
  for (int i = 0; i < num_nodes; ++i) {
    vector<LatticeArc>& alts = l[i];
    const Hypergraph::Node& node = g.nodes_[i];
    const int num_alts = node.out_edges_.size();
    alts.resize(num_alts);
    for (int j = 0; j < num_alts; ++j) {
      const Hypergraph::Edge& edge = g.edges_[node.out_edges_[j]];
      alts[j].label = edge.rule_->e_[1];
      alts[j].cost = edge.feature_values_.get(fid0);
      alts[j].dist2next = edge.head_node_ - node.id_;
    }
  }
}

void HypergraphIO::WriteAsCFG(const Hypergraph& hg) {
  vector<int> cats(hg.nodes_.size());
  // each node in the translation forest becomes a "non-terminal" in the new
  // grammar, create the labels here
  const string kSEP = "_";
  for (int i = 0; i < hg.nodes_.size(); ++i) {
    string pstr = "CAT";
    if (hg.nodes_[i].cat_ < 0)
      pstr = TD::Convert(-hg.nodes_[i].cat_);
    cats[i] = TD::Convert(pstr + kSEP + boost::lexical_cast<string>(i)) * -1;
  }

  for (int i = 0; i < hg.edges_.size(); ++i) {
    const Hypergraph::Edge& edge = hg.edges_[i];
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
    for (int j = 0; j < tgt.size(); ++j) {
      const WordID& cur = tgt[j];
      if (cur > 0) {
        f[j] = cur;
      } else {
        tn[ntc++] = cur;
        f[j] = cats[edge.tail_nodes_[-cur]];
      }
    }
    ntc = 0;
    for (int j = 0; j < src.size(); ++j) {
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
    cout << rule->AsString() << endl;
  }
}

/* Output format:
 * #vertices
 * for each vertex in bottom-up topological order:
 *   #downward_edges
 *   for each downward edge:
 *     RHS with [vertex_index] for NTs ||| scores
 */
void HypergraphIO::WriteTarget(const std::string &base, unsigned int id, const Hypergraph& hg) {
  std::string name(base);
  name += '/';
  name += boost::lexical_cast<std::string>(id);
  std::fstream out(name.c_str(), std::fstream::out);
  out << hg.nodes_.size() << ' ' << hg.edges_.size() << '\n';
  for (unsigned int i = 0; i < hg.nodes_.size(); ++i) {
    const Hypergraph::EdgesVector &edges = hg.nodes_[i].in_edges_;
    out << edges.size() << '\n';
    for (unsigned int j = 0; j < edges.size(); ++j) {
      const Hypergraph::Edge &edge = hg.edges_[edges[j]];
      const std::vector<WordID> &e = edge.rule_->e();
      for (std::vector<WordID>::const_iterator word = e.begin(); word != e.end(); ++word) {
        if (*word <= 0) {
          out << '[' << edge.tail_nodes_[-*word] << "] ";
        } else {
          out << TD::Convert(*word) << ' ';
        }
      }
      out << "||| " << edge.rule_->scores_ << '\n';
    }
  }
}

