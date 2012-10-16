#include "hg_io.h"

#include <fstream>
#include <sstream>
#include <iostream>

#include "fast_lexical_cast.hpp"

#include "tdict.h"
#include "json_parse.h"
#include "hg.h"

using namespace std;

struct HGReader : public JSONParser {
  HGReader(Hypergraph* g) : rp("[X] ||| "), state(-1), hg(*g), nodes_needed(true), edges_needed(true) { nodes = 0; edges = 0; }

  void CreateNode(const string& cat, const vector<int>& in_edges) {
    WordID c = TD::Convert("X") * -1;
    if (!cat.empty()) c = TD::Convert(cat) * -1;
    Hypergraph::Node* node = hg.AddNode(c);
    for (int i = 0; i < in_edges.size(); ++i) {
      if (in_edges[i] >= hg.edges_.size()) {
        cerr << "JSONParser: in_edges[" << i << "]=" << in_edges[i]
             << ", but hg only has " << hg.edges_.size() << " edges!\n";
        abort();
      }
      hg.ConnectEdgeToHeadNode(&hg.edges_[in_edges[i]], node);
    }
  }
  void CreateEdge(const TRulePtr& rule, SparseVector<double>* feats, const SmallVectorUnsigned& tail) {
    Hypergraph::Edge* edge = hg.AddEdge(rule, tail);
    feats->swap(edge->feature_values_);
    edge->i_ = spans[0];
    edge->j_ = spans[1];
    edge->prev_i_ = spans[2];
    edge->prev_j_ = spans[3];
  }

  bool HandleJSONEvent(int type, const JSON_value* value) {
    switch(state) {
    case -1:
      assert(type == JSON_T_OBJECT_BEGIN);
      state = 0;
      break;
    case 0:
      if (type == JSON_T_OBJECT_END) {
        //cerr << "HG created\n";  // TODO, signal some kind of callback
      } else if (type == JSON_T_KEY) {
        string val = value->vu.str.value;
        if (val == "features") { assert(fdict.empty()); state = 1; }
        else if (val == "is_sorted") { state = 3; }
        else if (val == "rules") { assert(rules.empty()); state = 4; }
        else if (val == "node") { state = 8; }
        else if (val == "edges") { state = 13; }
        else { cerr << "Unexpected key: " << val << endl; return false; }
      }
      break;

    // features
    case 1:
      if(type == JSON_T_NULL) { state = 0; break; }
      assert(type == JSON_T_ARRAY_BEGIN);
      state = 2;
      break;
    case 2:
      if(type == JSON_T_ARRAY_END) { state = 0; break; }
      assert(type == JSON_T_STRING);
      fdict.push_back(FD::Convert(value->vu.str.value));
      assert(fdict.back() > 0);
      break;

    // is_sorted
    case 3:
      assert(type == JSON_T_TRUE || type == JSON_T_FALSE);
      is_sorted = (type == JSON_T_TRUE);
      if (!is_sorted) { cerr << "[WARNING] is_sorted flag is ignored\n"; }
      state = 0;
      break;

    // rules
    case 4:
      if(type == JSON_T_NULL) { state = 0; break; }
      assert(type == JSON_T_ARRAY_BEGIN);
      state = 5;
      break;
    case 5:
      if(type == JSON_T_ARRAY_END) { state = 0; break; }
      assert(type == JSON_T_INTEGER);
      state = 6;
      rule_id = value->vu.integer_value;
      break;
    case 6:
      assert(type == JSON_T_STRING);
      rules[rule_id] = TRulePtr(new TRule(value->vu.str.value));
      state = 5;
      break;

    // Nodes
    case 8:
      assert(type == JSON_T_OBJECT_BEGIN);
      ++nodes;
      in_edges.clear();
      cat.clear();
      state = 9; break;
    case 9:
      if (type == JSON_T_OBJECT_END) {
        //cerr << "Creating NODE\n";
        CreateNode(cat, in_edges);
        state = 0; break;
      }
      assert(type == JSON_T_KEY);
      cur_key = value->vu.str.value;
      if (cur_key == "cat") { assert(cat.empty()); state = 10; break; }
      if (cur_key == "in_edges") { assert(in_edges.empty()); state = 11; break; }
      cerr << "Syntax error: unexpected key " << cur_key << " in node specification.\n";
      return false;
    case 10:
      assert(type == JSON_T_STRING || type == JSON_T_NULL);
      cat = value->vu.str.value;
      state = 9; break;
    case 11:
      if (type == JSON_T_NULL) { state = 9; break; }
      assert(type == JSON_T_ARRAY_BEGIN);
      state = 12; break;
    case 12:
      if (type == JSON_T_ARRAY_END) { state = 9; break; }
      assert(type == JSON_T_INTEGER);
      //cerr << "in_edges: " << value->vu.integer_value << endl;
      in_edges.push_back(value->vu.integer_value);
      break;

    //   "edges": [ { "tail": null, "feats" : [0,1.63,1,-0.54], "rule": 12},
    //         { "tail": null, "feats" : [0,0.87,1,0.02], "spans":[1,2,3,4], "rule": 17},
    //         { "tail": [0], "feats" : [1,2.3,2,15.3,"ExtraFeature",1.2], "rule": 13}]
    case 13:
      assert(type == JSON_T_ARRAY_BEGIN);
      state = 14;
      break;
    case 14:
      if (type == JSON_T_ARRAY_END) { state = 0; break; }
      assert(type == JSON_T_OBJECT_BEGIN);
      //cerr << "New edge\n";
      ++edges;
      cur_rule.reset(); feats.clear(); tail.clear();
      state = 15; break;
    case 15:
      if (type == JSON_T_OBJECT_END) {
        CreateEdge(cur_rule, &feats, tail);
        state = 14; break;
      }
      assert(type == JSON_T_KEY);
      cur_key = value->vu.str.value;
      //cerr << "edge key " << cur_key << endl;
      if (cur_key == "rule") { assert(!cur_rule); state = 16; break; }
      if (cur_key == "spans") { assert(!cur_rule); state = 22; break; }
      if (cur_key == "feats") { assert(feats.empty()); state = 17; break; }
      if (cur_key == "tail") { assert(tail.empty()); state = 20; break; }
      cerr << "Unexpected key " << cur_key << " in edge specification\n";
      return false;
    case 16:    // edge.rule
      if (type == JSON_T_INTEGER) {
        int rule_id = value->vu.integer_value;
        if (rules.find(rule_id) == rules.end()) {
          // rules list must come before the edge definitions!
          cerr << "Rule_id " << rule_id << " given but only loaded " << rules.size() << " rules\n";
          return false;
        }
        cur_rule = rules[rule_id];
      } else if (type == JSON_T_STRING) {
        cur_rule.reset(new TRule(value->vu.str.value));
      } else {
        cerr << "Rule must be either a rule id or a rule string" << endl;
        return false;
      }
      // cerr << "Edge: rule=" << cur_rule->AsString() << endl;
      state = 15;
      break;
    case 17:      // edge.feats
      if (type == JSON_T_NULL) { state = 15; break; }
      assert(type == JSON_T_ARRAY_BEGIN);
      state = 18; break;
    case 18:
      if (type == JSON_T_ARRAY_END) { state = 15; break; }
      if (type != JSON_T_INTEGER && type != JSON_T_STRING) {
        cerr << "Unexpected feature id type\n"; return false;
      }
      if (type == JSON_T_INTEGER) {
        fid = value->vu.integer_value;
        assert(fid < fdict.size());
        fid = fdict[fid];
      } else if (JSON_T_STRING) {
        fid = FD::Convert(value->vu.str.value);
      } else { abort(); }
      state = 19;
      break;
    case 19:
      {
        assert(type == JSON_T_INTEGER || type == JSON_T_FLOAT);
        double val = (type == JSON_T_INTEGER ? static_cast<double>(value->vu.integer_value) :
	                                       strtod(value->vu.str.value, NULL));
        feats.set_value(fid, val);
        state = 18;
        break;
      }
    case 20:     // edge.tail
      if (type == JSON_T_NULL) { state = 15; break; }
      assert(type == JSON_T_ARRAY_BEGIN);
      state = 21; break;
    case 21:
      if (type == JSON_T_ARRAY_END) { state = 15; break; }
      assert(type == JSON_T_INTEGER);
      tail.push_back(value->vu.integer_value);
      break;
    case 22:     // edge.spans
      assert(type == JSON_T_ARRAY_BEGIN);
      state = 23;
      spans[0] = spans[1] = spans[2] = spans[3] = -1;
      spanc = 0;
      break;
    case 23:
      if (type == JSON_T_ARRAY_END) { state = 15; break; }
      assert(type == JSON_T_INTEGER);
      assert(spanc < 4);
      spans[spanc] = value->vu.integer_value;
      ++spanc;
    }
    return true;
  }
  string rp;
  string cat;
  SmallVectorUnsigned tail;
  vector<int> in_edges;
  TRulePtr cur_rule;
  map<int, TRulePtr> rules;
  vector<int> fdict;
  SparseVector<double> feats;
  int state;
  int fid;
  int nodes;
  int edges;
  int spans[4];
  int spanc;
  string cur_key;
  Hypergraph& hg;
  int rule_id;
  bool nodes_needed;
  bool edges_needed;
  bool is_sorted;
};

bool HypergraphIO::ReadFromJSON(istream* in, Hypergraph* hg) {
  hg->clear();
  HGReader reader(hg);
  return reader.Parse(in);
}

static void WriteRule(const TRule& r, ostream* out) {
  if (!r.lhs_) { (*out) << "[X] ||| "; }
  JSONParser::WriteEscapedString(r.AsString(), out);
}

bool HypergraphIO::WriteToJSON(const Hypergraph& hg, bool remove_rules, ostream* out) {
  if (hg.empty()) { *out << "{}\n"; return true; }
  map<const TRule*, int> rid;
  ostream& o = *out;
  rid[NULL] = 0;
  o << '{';
  if (!remove_rules) {
    o << "\"rules\":[";
    for (int i = 0; i < hg.edges_.size(); ++i) {
      const TRule* r = hg.edges_[i].rule_.get();
      int &id = rid[r];
      if (!id) {
        id=rid.size() - 1;
        if (id > 1) o << ',';
        o << id << ',';
        WriteRule(*r, &o);
      };
    }
    o << "],";
  }
  const bool use_fdict = FD::NumFeats() < 1000;
  if (use_fdict) {
    o << "\"features\":[";
    for (int i = 1; i < FD::NumFeats(); ++i) {
      o << (i==1 ? "":",");
      JSONParser::WriteEscapedString(FD::Convert(i), &o);
    }
    o << "],";
  }
  vector<int> edgemap(hg.edges_.size(), -1);  // edges may be in non-topo order
  int edge_count = 0;
  for (int i = 0; i < hg.nodes_.size(); ++i) {
    const Hypergraph::Node& node = hg.nodes_[i];
    if (i > 0) { o << ","; }
    o << "\"edges\":[";
    for (int j = 0; j < node.in_edges_.size(); ++j) {
      const Hypergraph::Edge& edge = hg.edges_[node.in_edges_[j]];
      edgemap[edge.id_] = edge_count;
      ++edge_count;
      o << (j == 0 ? "" : ",") << "{";

      o << "\"tail\":[";
      for (int k = 0; k < edge.tail_nodes_.size(); ++k) {
        o << (k > 0 ? "," : "") << edge.tail_nodes_[k];
      }
      o << "],";

      o << "\"spans\":[" << edge.i_ << "," << edge.j_ << "," << edge.prev_i_ << "," << edge.prev_j_ << "],";

      o << "\"feats\":[";
      bool first = true;
      for (SparseVector<double>::const_iterator it = edge.feature_values_.begin(); it != edge.feature_values_.end(); ++it) {
        if (!it->second) continue;   // don't write features that have a zero value
        if (!it->first) continue;    // if the feature set was frozen this might happen
        if (!first) o << ',';
        if (use_fdict)
          o << (it->first - 1);
        else {
	  JSONParser::WriteEscapedString(FD::Convert(it->first), &o);
        }
	o << ',' << it->second;
        first = false;
      }
      o << "]";
      if (!remove_rules) { o << ",\"rule\":" << rid[edge.rule_.get()]; }
      o << "}";
    }
    o << "],";

    o << "\"node\":{\"in_edges\":[";
    for (int j = 0; j < node.in_edges_.size(); ++j) {
      int mapped_edge = edgemap[node.in_edges_[j]];
      assert(mapped_edge >= 0);
      o << (j == 0 ? "" : ",") << mapped_edge;
    }
    o << "]";
    if (node.cat_ < 0) {
       o << ",\"cat\":";
       JSONParser::WriteEscapedString(TD::Convert(node.cat_ * -1), &o);
    }
    o << "}";
  }
  o << "}\n";
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

