//TODO: lazily generate feature vectors for hyperarcs (because some of them will be pruned).  this means 1) storing ref to rule for those features 2) providing ff interface for regenerating its feature vector from hyperedge+states and probably 3) still caching feat. vect on hyperedge once it's been generated.  ff would normally just contribute its weighted score and result state, not component features.  however, the hypergraph drops the state used by ffs after rescoring is done, so recomputation would have to start at the leaves and work bottom up.  question: which takes more space, feature id+value, or state?

#include "hg.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <set>
#include <map>
#include <iostream>
#include <sstream>

#include "viterbi.h"
#include "inside_outside.h"
#include "tdict.h"
#include "verbose.h"

using namespace std;

#if 0
Hypergraph::Edge const* Hypergraph::ViterbiGoalEdge() const
{
  Edge const* r=0;
  for (unsigned i=0,e=edges_.size();i<e;++i) {
    Edge const& e=edges_[i];
    if (e.rule_ && e.rule_->IsGoal() && (!r || e.edge_prob_ > r->edge_prob_))
      r=&e;
  }
  return r;
}
#endif

Hypergraph::Edge const* Hypergraph::ViterbiSortInEdges()
{
  NodeProbs nv;
  ComputeNodeViterbi(&nv);
  return SortInEdgesByNodeViterbi(nv);
}

Hypergraph::Edge const* Hypergraph::SortInEdgesByNodeViterbi(NodeProbs const& nv)
{
  EdgeProbs ev;
  ComputeEdgeViterbi(nv,&ev);
  return ViterbiSortInEdges(ev);
}

namespace {
struct less_ve {
  Hypergraph::EdgeProbs const& ev;
  //Hypergraph const& hg;
  explicit less_ve(Hypergraph::EdgeProbs const& ev/*,Hypergraph const& hg */) : ev(ev)/*,hg(hg)*/ {  }
  bool operator()(int a,int b) const {
    return ev[a] > ev[b];
  }
};
}

Hypergraph::Edge const* Hypergraph::ViterbiSortInEdges(EdgeProbs const& ev)
{
  for (unsigned i=0;i<nodes_.size();++i) {
    EdgesVector &ie=nodes_[i].in_edges_;
    std::sort(ie.begin(),ie.end(),less_ve(ev));
  }
  return first_edge();
}

prob_t Hypergraph::ComputeEdgeViterbi(EdgeProbs *ev) const {
  NodeProbs nv;
  ComputeNodeViterbi(&nv);
  return ComputeEdgeViterbi(nv,ev);
}

prob_t Hypergraph::ComputeEdgeViterbi(NodeProbs const& nv,EdgeProbs *ev) const {
  unsigned ne=edges_.size();
  ev->resize(ne);
  for (unsigned i=0;i<ne;++i) {
    Edge const& e=edges_[i];
    prob_t r=e.edge_prob_;
    TailNodeVector const& t=e.tail_nodes_;
    for (TailNodeVector::const_iterator j=t.begin(),jj=t.end();j!=jj;++j)
      r*=nv[*j];
    /*
    for (int j=0;j<e.tail_nodes_.size();++j)
      r*=nv[e.tail_nodes_[j]];
    */
    (*ev)[i]=r;
  }
  return nv.empty()?prob_t(0):nv.back();
}


std::string Hypergraph::stats(std::string const& name) const
{
  ostringstream o;
  o<<name<<" (nodes/edges): "<<nodes_.size()<<'/'<<edges_.size()<<endl;
  o<<name<<"       (paths): "<<NumberOfPaths()<<endl;
  return o.str();
}


double Hypergraph::NumberOfPaths() const {
  return Inside<double, TransitionCountWeightFunction>(*this);
}

struct ScaledTransitionEventWeightFunction {
  typedef SparseVector<prob_t> Result;
  ScaledTransitionEventWeightFunction(double alpha) : scale_(alpha) {}
  inline SparseVector<prob_t> operator()(const Hypergraph::Edge& e) const {
    SparseVector<prob_t> result;
    result.set_value(e.id_, e.edge_prob_.pow(scale_));
    return result;
  }
  const double scale_;
};

// safe to reinterpret a vector of these as a vector of prob_t (plain old data)
struct TropicalValue {
  TropicalValue() : v_() {}
  TropicalValue(int v) {
    if (v == 0) v_ = prob_t::Zero();
    else if (v == 1) v_ = prob_t::One();
    else { cerr << "Bad value in TropicalValue(int).\n"; abort(); }
  }
  TropicalValue(unsigned v) : v_(v) {}
  TropicalValue(const prob_t& v) : v_(v) {}
//  operator prob_t() const { return v_; }
  inline TropicalValue& operator+=(const TropicalValue& o) {
    if (v_ < o.v_) v_ = o.v_;
    return *this;
  }
  inline TropicalValue& operator*=(const TropicalValue& o) {
    v_ *= o.v_;
    return *this;
  }
  inline bool operator==(const TropicalValue& o) const { return v_ == o.v_; }
  prob_t v_;
};

struct ViterbiWeightFunction {
  typedef TropicalValue Weight;
  inline TropicalValue operator()(const Hypergraph::Edge& e) const {
    return TropicalValue(e.edge_prob_);
  }
};

struct ViterbiTransitionEventWeightFunction {
  typedef SparseVector<TropicalValue> Result;
  inline SparseVector<TropicalValue> operator()(const Hypergraph::Edge& e) const {
    SparseVector<TropicalValue> result;
    result.set_value(e.id_, TropicalValue(e.edge_prob_));
    return result;
  }
};


//TODO: both Compute* methods build sparse vectors with size = whole subhypergraph, for every node.  there's no need for that.
prob_t Hypergraph::ComputeEdgePosteriors(double scale, vector<prob_t>* posts) const {
  const ScaledEdgeProb weight(scale);
  const ScaledTransitionEventWeightFunction w2(scale);
  SparseVector<prob_t> pv;
  const prob_t inside = InsideOutside<prob_t,
                  ScaledEdgeProb,
                  SparseVector<prob_t>,
                  ScaledTransitionEventWeightFunction>(*this, &pv, weight, w2);
  posts->resize(edges_.size());
  for (unsigned i = 0; i < edges_.size(); ++i)
    (*posts)[i] = prob_t(pv.value(i));
  return inside;
}

prob_t Hypergraph::ComputeBestPathThroughEdges(vector<prob_t>* post) const {
  // I don't like this - explicitly passing around counts of each edge.  It's clever but slow.
  SparseVector<TropicalValue> pv;
  const TropicalValue viterbi_weight = InsideOutside<TropicalValue,
                  ViterbiWeightFunction,
                  SparseVector<TropicalValue>,
                  ViterbiTransitionEventWeightFunction>(*this, &pv);
  post->resize(edges_.size());
  for (unsigned i = 0; i < edges_.size(); ++i)
    (*post)[i] = pv.value(i).v_;
  return viterbi_weight.v_;
}

void Hypergraph::PushWeightsToSource(double scale) {
  vector<prob_t> posts;
  ComputeEdgePosteriors(scale, &posts);
  for (unsigned i = 0; i < nodes_.size(); ++i) {
    const Hypergraph::Node& node = nodes_[i];
    prob_t z = prob_t::Zero();
    for (unsigned j = 0; j < node.out_edges_.size(); ++j)
      z += posts[node.out_edges_[j]];
    for (unsigned j = 0; j < node.out_edges_.size(); ++j) {
      edges_[node.out_edges_[j]].edge_prob_ = posts[node.out_edges_[j]] / z;
    }
  }
}

namespace {
struct vpusher : public vector<TropicalValue> {
  int fid;
  vpusher(int fid=0) : fid(fid) {  }
  void operator()(int n,int /*ei*/,Hypergraph::Edge &e) const {
    Hypergraph::TailNodeVector const& t=e.tail_nodes_;
    prob_t p=e.edge_prob_;
    for (unsigned i=0;i<t.size();++i)
      p*=(*this)[t[i]].v_;
    e.feature_values_.set_value(fid,log(e.edge_prob_=p/(*this)[n].v_));
  }
};
}

prob_t Hypergraph::ComputeNodeViterbi(NodeProbs *np) const
{
  return Inside(*this,
                reinterpret_cast<std::vector<TropicalValue> *>(np),
                ViterbiWeightFunction()).v_;
}


// save pushed weight ot some fid if we want.  0 = don't care
prob_t Hypergraph::PushViterbiWeightsToGoal(int fid) {
  vpusher vi(fid);
  NodeProbs np;
  prob_t r=ComputeNodeViterbi(&np);
  visit_edges(vi);
  return r;
}


prob_t Hypergraph::PushWeightsToGoal(double scale) {
  vector<prob_t> posts;
  const prob_t inside_z = ComputeEdgePosteriors(scale, &posts);
  for (unsigned i = 0; i < nodes_.size(); ++i) {
    const Hypergraph::Node& node = nodes_[i];
    prob_t z = prob_t::Zero();
    for (unsigned j = 0; j < node.in_edges_.size(); ++j)
      z += posts[node.in_edges_[j]];
    for (unsigned j = 0; j < node.in_edges_.size(); ++j) {
      edges_[node.in_edges_[j]].edge_prob_ = posts[node.in_edges_[j]] / z;
    }
  }
  return inside_z;
}

struct EdgeExistsWeightFunction {
  EdgeExistsWeightFunction(const vector<bool>& prunes) : prunes_(prunes) {}
  bool operator()(const Hypergraph::Edge& edge) const {
    return !prunes_[edge.id_];
  }
 private:
  const vector<bool>& prunes_;
};

void Hypergraph::PruneEdges(const EdgeMask& prune_edge, bool run_inside_algorithm) {
  assert(prune_edge.size() == edges_.size());
  vector<bool> filtered = prune_edge;

  if (run_inside_algorithm) {
    const EdgeExistsWeightFunction wf(prune_edge);
    vector<Boolean> reachable;
    bool goal_derivable = Inside<Boolean, EdgeExistsWeightFunction>(*this, &reachable, wf);
    if (!goal_derivable) {
      edges_.clear();
      nodes_.clear();
      nodes_.push_back(Node());
      return;
    }

    assert(reachable.size() == nodes_.size());
    for (unsigned i = 0; i < edges_.size(); ++i) {
      bool prune = prune_edge[i];
      if (!prune) {
        const Edge& edge = edges_[i];
        for (unsigned j = 0; j < edge.tail_nodes_.size(); ++j) {
          if (!reachable[edge.tail_nodes_[j]]) {
            prune = true;
            break;
          }
        }
      }
      filtered[i] = prune;
    }
  }
  TopologicallySortNodesAndEdges(nodes_.size() - 1, &filtered);
}


// drop edges w/ max marginal prob less than cutoff.  this means that bigger cutoff is stricter.
void Hypergraph::MarginPrune(vector<prob_t> const& io,prob_t cutoff,vector<bool> const* preserve_mask,bool safe_inside,bool verbose)
{
  assert(io.size()==edges_.size());

  //TODO: //FIXME: if EPSILON is 0, then remnants (useless edges that don't connect to top? or top-connected but not bottom-up buildable referenced?) are left in the hypergraph output that cause mr_vest_map to segfault.  adding EPSILON probably just covers up the symptom by making it far less frequent; I imagine any time threshold is set by DensityPrune, cutoff is exactly equal to the io of several nodes, but because of how it's computed, some round slightly down vs. slightly up.  probably the flaw is in PruneEdges.

  const prob_t creep=abslog(cutoff).pow(1e-6); // some barely >1 (small positive log) ratio.  linear in logspace of course // start more permissive, then become less generous.  this is barely more than 1.  we want to do this because it's a disaster if something lower in a derivation tree is deleted, but the higher thing remains (unless safe_inside)

  vector<bool> prune(edges_.size());
  if (verbose) {
    if (preserve_mask) cerr << preserve_mask->size() << " " << prune.size() << endl;
    cerr<<"Finishing prune for "<<prune.size()<<" edges; CUTOFF=" << cutoff << endl;
  }
  unsigned pc = 0;
  for (unsigned i = 0; i < io.size(); ++i) {
    cutoff*=creep; // start more permissive, then become less generous.  this is barely more than 1.  we want to do this because it's a disaster if something lower in a derivation tree is deleted, but the higher thing remains (unless safe_inside)
    const bool prune_edge = (io[i] < cutoff);
    if (prune_edge) {
      ++pc;
      prune[i] = !(preserve_mask && (*preserve_mask)[i]);
    }
  }
  if (verbose)
    cerr << "Finished pruning; removed " << pc << "/" << io.size() << " edges\n";
  PruneEdges(prune,safe_inside); // inside reachability check in case cutoff rounded down too much (probably redundant with EPSILON hack)
}

template <class V>
V nth_greatest(int n,vector<V> vs) {
  nth_element(vs.begin(),vs.begin()+n,vs.end(),greater<V>());
  return vs[n];
}

bool Hypergraph::PruneInsideOutside(double alpha,double density,const EdgeMask* preserve_mask,const bool use_sum_prod_semiring, const double scale,bool safe_inside)
{
  bool use_density=density!=0;
  bool use_beam=alpha!=0;
  assert(!use_beam||alpha>0);
  assert(!use_density||density>=1);
  assert(!use_sum_prod_semiring||scale>0);
  unsigned rnum=edges_.size();
  if (use_density) {
    const int plen = ViterbiPathLength(*this);
    vector<WordID> bp;
    rnum = min(rnum, static_cast<unsigned>(density * plen));
    if (!SILENT) cerr << "Density pruning: keep "<<rnum<<" of "<<edges_.size()<<" edges (viterbi = "<<plen<<" edges)"<<endl;
    if (rnum == edges_.size()) {
      if (!SILENT) cerr << "No pruning required: denisty already sufficient\n";
      if (!use_beam)
        return false;
      use_density=false;
    }
  }
  assert(use_density||use_beam);
  InsideOutsides<prob_t> io;
  OutsideNormalize<prob_t> norm;
  if (use_sum_prod_semiring)
    io.compute(*this,norm,ScaledEdgeProb(scale));
  else
    io.compute(*this,norm,ViterbiWeightFunction());  // the storage gets cast to Tropical from prob_t, scary - e.g. w/ specialized static allocator differences it could break.
  vector<prob_t> mm;
  io.compute_edge_marginals(*this,mm,EdgeProb()); // should be normalized to 1 for best edges in viterbi.  in sum, best is less than 1.

  prob_t cutoff=prob_t::One(); // we'll destroy everything smaller than this (note: nothing is bigger than 1).  so bigger cutoff = more pruning.
  bool density_won=false;
  if (use_density) {
    cutoff=nth_greatest(rnum,mm);
    density_won=true;
  }
  if (use_beam) {
    prob_t best=prob_t::One();
    if (use_sum_prod_semiring) {
      for (unsigned i = 0; i < mm.size(); ++i)
        if (mm[i] > best) best = mm[i];
    }
    prob_t beam_cut=best*prob_t::exp(-alpha);
    if (!(use_density&&cutoff>beam_cut)) {
      density_won=false;
      cutoff=beam_cut;
    }
  }
  MarginPrune(mm,cutoff,preserve_mask,safe_inside);
  return density_won;
}


void Hypergraph::PrintGraphviz() const {
  int ei = 0;
  cerr << "digraph G {\n  rankdir=LR;\n  nodesep=.05;\n";
  for (vector<Edge>::const_iterator i = edges_.begin();
       i != edges_.end(); ++i) {
    const Edge& edge=*i;
    ++ei;
    static const string none = "<null>";
    string rule = (edge.rule_ ? edge.rule_->AsString(false) : none);

    cerr << "   A_" << ei << " [label=\"" << rule << " p=" << edge.edge_prob_
         << " F:" << edge.feature_values_
         << "\" shape=\"rect\"];\n";
    Hypergraph::TailNodeVector indorder(edge.tail_nodes_.size(), 0);
    int ntc = 0;
    for (unsigned i = 0; i < edge.rule_->e_.size(); ++i) {
      if (edge.rule_->e_[i] <= 0) indorder[ntc++] = 1 + (-1 * edge.rule_->e_[i]);
    }
    for (unsigned i = 0; i < edge.tail_nodes_.size(); ++i) {
      cerr << "     " << edge.tail_nodes_[i] << " -> A_" << ei;
      if (edge.tail_nodes_.size() > 1) {
        cerr << " [label=\"" << indorder[i] << "\"]";
      }
      cerr << ";\n";
    }
    cerr << "     A_" << ei << " -> " << edge.head_node_ << ";\n";
  }
  for (vector<Node>::const_iterator ni = nodes_.begin();
      ni != nodes_.end(); ++ni) {
    cerr << "  " << ni->id_ << "[label=\"" << (ni->cat_ < 0 ? TD::Convert(ni->cat_ * -1) : "")
    //cerr << "  " << ni->id_ << "[label=\"" << ni->cat_
         << " n=" << ni->id_
//         << ",x=" << &*ni
//         << ",in=" << ni->in_edges_.size()
//         << ",out=" << ni->out_edges_.size()
         << "\"];\n";
  }
  cerr << "}\n";
}

void Hypergraph::Union(const Hypergraph& other) {
  if (&other == this) return;
  if (nodes_.empty()) { nodes_ = other.nodes_; edges_ = other.edges_; return; }
  unsigned noff = nodes_.size();
  unsigned eoff = edges_.size();
  int ogoal = other.nodes_.size() - 1;
  int cgoal = noff - 1;
  // keep a single goal node, so add nodes.size - 1
  nodes_.resize(nodes_.size() + ogoal);
  // add all edges
  edges_.resize(edges_.size() + other.edges_.size());

  for (int i = 0; i < ogoal; ++i) {
    const Node& on = other.nodes_[i];
    Node& cn = nodes_[i + noff];
    cn.id_ = i + noff;
    cn.in_edges_.resize(on.in_edges_.size());
    for (unsigned j = 0; j < on.in_edges_.size(); ++j)
      cn.in_edges_[j] = on.in_edges_[j] + eoff;

    cn.out_edges_.resize(on.out_edges_.size());
    for (unsigned j = 0; j < on.out_edges_.size(); ++j)
      cn.out_edges_[j] = on.out_edges_[j] + eoff;
  }

  for (unsigned i = 0; i < other.edges_.size(); ++i) {
    const Edge& oe = other.edges_[i];
    Edge& ce = edges_[i + eoff];
    ce.id_ = i + eoff;
    ce.rule_ = oe.rule_;
    ce.feature_values_ = oe.feature_values_;
    if (oe.head_node_ == ogoal) {
      ce.head_node_ = cgoal;
      nodes_[cgoal].in_edges_.push_back(ce.id_);
    } else {
      ce.head_node_ = oe.head_node_ + noff;
    }
    ce.tail_nodes_.resize(oe.tail_nodes_.size());
    for (unsigned j = 0; j < oe.tail_nodes_.size(); ++j)
      ce.tail_nodes_[j] = oe.tail_nodes_[j] + noff;
  }

  TopologicallySortNodesAndEdges(cgoal);
}

void Hypergraph::PruneUnreachable(int goal_node_id) {
  TopologicallySortNodesAndEdges(goal_node_id, NULL);
}

struct DFSContext {
  int node;
  int edge_iter;
  int tail_iter;
  DFSContext(int n, int e, int t) : node(n), edge_iter(e), tail_iter(t) {}
};

enum ColorType { WHITE, GRAY, BLACK };

template <class T>
struct BadId {
  bool operator()(const T& obj) const { return obj.id_ == -1; }
};

template <class T>
struct IdCompare {
  bool operator()(const T& a, const T& b) { return a.id_ < b.id_; }
};

// this keeps the nodes' edge indices and edges' node indices in sync.  or do nodes not get removed when you prune_edges?  seems like they get reordered.
//TODO: if you had parallel arrays associating data w/ each node or edge, you'd want access to reloc_node and reloc_edge - expose in stateful object?
void Hypergraph::TopologicallySortNodesAndEdges(int goal_index,
                                                const vector<bool>* prune_edges) {
  edges_topo_=true;
  // figure out which nodes are reachable from the goal
  vector<int> reloc_node(nodes_.size(), -1);
  vector<int> reloc_edge(edges_.size(), -1);
  vector<ColorType> color(nodes_.size(), WHITE);
  vector<DFSContext> stack;
  stack.reserve(nodes_.size());
  stack.push_back(DFSContext(goal_index, 0, 0));
  int node_count = 0;
  int edge_count = 0;
  while(!stack.empty()) {
    const DFSContext& p = stack.back();
    int cur_ni = p.node;
    int edge_i = p.edge_iter;
    int tail_i = p.tail_iter;
    stack.pop_back();
    const Node* cur_node = &nodes_[cur_ni];
    int edge_end = cur_node->in_edges_.size();
    while (edge_i != edge_end) {
      const Edge& cur_edge = edges_[cur_node->in_edges_[edge_i]];
      const int tail_end = cur_edge.tail_nodes_.size();
      if ((tail_end == tail_i) || (prune_edges && (*prune_edges)[cur_edge.id_])) {
        ++edge_i;
        tail_i = 0;
        continue;
      }
      const int tail_ni = cur_edge.tail_nodes_[tail_i];
      const int tail_color = color[tail_ni];
      if (tail_color == WHITE) {
        stack.push_back(DFSContext(cur_ni, edge_i, ++tail_i));
        cur_ni = tail_ni;
        cur_node = &nodes_[cur_ni];
        color[cur_ni] = GRAY;
        edge_i = 0;
        edge_end = cur_node->in_edges_.size();
        tail_i = 0;
      } else if (tail_color == BLACK) {
        ++tail_i;
      } else if (tail_color == GRAY) {
        // this can happen if, e.g., it is possible to rederive
        // a single cell in the CKY chart via a cycle.
        cerr << "Detected forbidden cycle in HG:\n";
        cerr << "  " << cur_edge.rule_->AsString() << endl;
        while(!stack.empty()) {
          const DFSContext& p = stack.back();
          cerr << "  " << edges_[nodes_[p.node].in_edges_[p.edge_iter]].rule_->AsString() << endl;
          stack.pop_back();
        }
        abort();
      }
    }
    color[cur_ni] = BLACK;
    reloc_node[cur_ni] = node_count++;
    if (prune_edges) {
      for (int i = 0; i < edge_end; ++i) {
        int ei = cur_node->in_edges_[i];
        if (!(*prune_edges)[ei])
          reloc_edge[cur_node->in_edges_[i]] = edge_count++;
      }
    } else {
      for (int i = 0; i < edge_end; ++i)
        reloc_edge[cur_node->in_edges_[i]] = edge_count++;
    }
  }
#ifndef HG_EDGES_TOPO_SORTED
  int ec = 0;
  for (unsigned i = 0; i < reloc_edge.size(); ++i) {
    int& cp = reloc_edge[i];
    if (cp >= 0) { cp = ec++; }
  }
#endif

#if 0
  cerr << "TOPO:";
  for (int i = 0; i < reloc_node.size(); ++i)
    cerr << " " << reloc_node[i];
  cerr << endl;
  cerr << "EDGE:";
  for (int i = 0; i < reloc_edge.size(); ++i)
    cerr << " " << reloc_edge[i];
  cerr << endl;
#endif
  bool no_op = true;
  for (unsigned i = 0; i < reloc_node.size() && no_op; ++i)
    if (reloc_node[i] != static_cast<int>(i)) no_op = false;
  for (unsigned i = 0; i < reloc_edge.size() && no_op; ++i)
    if (reloc_edge[i] != static_cast<int>(i)) no_op = false;
  if (no_op) return;
  for (unsigned i = 0; i < reloc_node.size(); ++i) {
    Node& node = nodes_[i];
    node.id_ = reloc_node[i];
    int c = 0;
    for (unsigned j = 0; j < node.in_edges_.size(); ++j) {
      const int new_index = reloc_edge[node.in_edges_[j]];
      if (new_index >= 0)
        node.in_edges_[c++] = new_index;
    }
    node.in_edges_.resize(c);
    c = 0;
    for (unsigned j = 0; j < node.out_edges_.size(); ++j) {
      const int new_index = reloc_edge[node.out_edges_[j]];
      if (new_index >= 0)
        node.out_edges_[c++] = new_index;
    }
    node.out_edges_.resize(c);
  }
  for (unsigned i = 0; i < reloc_edge.size(); ++i) {
    Edge& edge = edges_[i];
    edge.id_ = reloc_edge[i];
    edge.head_node_ = reloc_node[edge.head_node_];
    for (unsigned j = 0; j < edge.tail_nodes_.size(); ++j)
      edge.tail_nodes_[j] = reloc_node[edge.tail_nodes_[j]];
  }
  edges_.erase(remove_if(edges_.begin(), edges_.end(), BadId<Edge>()), edges_.end());
  nodes_.erase(remove_if(nodes_.begin(), nodes_.end(), BadId<Node>()), nodes_.end());
  sort(nodes_.begin(), nodes_.end(), IdCompare<Node>());
#ifndef HG_EDGES_TOPO_SORTED
  sort(edges_.begin(), edges_.end(), IdCompare<Edge>());
#endif
}

struct EdgeWeightSorter {
  const Hypergraph& hg;
  EdgeWeightSorter(const Hypergraph& h) : hg(h) {}
  bool operator()(int a, int b) const {
    return hg.edges_[a].edge_prob_ > hg.edges_[b].edge_prob_;
  }
};
/*
  void Hypergraph::SortInEdgesByEdgeWeights() {
  for (int i = 0; i < nodes_.size(); ++i) {
  Node& node = nodes_[i];
  sort(node.in_edges_.begin(), node.in_edges_.end(), EdgeWeightSorter(*this));
  }
  }
*/

std::string Hypergraph::show_first_tree(bool indent,int show_mask,int maxdepth,int depth) const {
  EdgeHandle fe=first_edge();
  return fe ? fe->derivation_tree(*this,fe,indent,show_mask,maxdepth,depth) : "";
}

std::string Hypergraph::show_viterbi_tree(bool indent,int show_mask,int maxdepth,int depth) const {
  HypergraphP v=CreateViterbiHypergraph();
//  cerr<<viterbi_stats(*v,"debug show_viterbi_tree",true,true,false)<<endl;
  return v->show_first_tree(indent,show_mask,maxdepth,depth);
}

HypergraphP Hypergraph::CreateEdgeSubset(EdgeMask &keep_edges) const {
  NodeMask kn;
  return CreateEdgeSubset(keep_edges,kn);
}

HypergraphP Hypergraph::CreateEdgeSubset(EdgeMask &keep_edges,NodeMask &kn) const {
  kn.clear();
  kn.resize(nodes_.size());
  for (unsigned n=0;n<nodes_.size();++n) { // this nested iteration gives us edges in topo order too
    EdgesVector const& es=nodes_[n].in_edges_;
    for (unsigned i=0;i<es.size();++i) {
      int ei=es[i];
      if (keep_edges[ei]) {
        const Edge& e = edges_[ei];
        TailNodeVector const& tails=e.tail_nodes_;
        for (unsigned j=0;j<e.tail_nodes_.size();++j) {
          if (!kn[tails[j]]) {
            keep_edges[ei]=false;
            goto next_edge;
          }
        }
        kn[e.head_node_]=true;
      }
    next_edge: ;
    }
  }
  return CreateNodeEdgeSubset(kn,keep_edges);
}

HypergraphP Hypergraph::CreateNodeEdgeSubset(NodeMask const& keep_nodes,EdgeMask const&keep_edges) const {
  indices_after n2(keep_nodes);
  indices_after e2(keep_edges);
  HypergraphP ret(new Hypergraph(n2.n_kept, e2.n_kept, is_linear_chain_));
  Nodes &rn=ret->nodes_;
  for (unsigned i=0;i<nodes_.size();++i)
    if (n2.keeping(i))
      rn[n2[i]].copy_reindex(nodes_[i],n2,e2);
  Edges &re=ret->edges_;
  for (unsigned i=0;i<edges_.size();++i)
    if (e2.keeping(i))
      re[e2[i]].copy_reindex(edges_[i],n2,e2);
  return ret;
}

void Hypergraph::TightenEdgeMask(EdgeMask &ke,NodeMask const& kn) const
{
  for (unsigned i = 0; i < edges_.size(); ++i) {
    if (ke[i]) {
      const Edge& edge = edges_[i];
      TailNodeVector const& tails=edge.tail_nodes_;
      for (unsigned j = 0; j < edge.tail_nodes_.size(); ++j) {
        if (!kn[tails[j]]) {
          ke[i]=false;
          goto next_edge;
        }
      }
    }
    next_edge:;
  }
}

void Hypergraph::set_ids() {
  for (unsigned i = 0; i < edges_.size(); ++i)
    edges_[i].id_=i;
  for (unsigned i = 0; i < nodes_.size(); ++i)
    nodes_[i].id_=i;
}

void Hypergraph::check_ids() const
{
  for (unsigned i = 0; i < edges_.size(); ++i)
    assert(edges_[i].id_==static_cast<int>(i));
  for (unsigned i = 0; i < nodes_.size(); ++i)
    assert(nodes_[i].id_==static_cast<int>(i));
}

HypergraphP Hypergraph::CreateViterbiHypergraph(const vector<bool>* edges) const {
  typedef ViterbiPathTraversal::Result VE;
  VE vit_edges;
  if (edges) {
    assert(edges->size() == edges_.size());
    Viterbi(*this, &vit_edges, ViterbiPathTraversal(), EdgeSelectEdgeWeightFunction(*edges));
  } else {
    Viterbi(*this, &vit_edges, ViterbiPathTraversal() ,EdgeProb());
  }
#if 1
# if 1
  check_ids();
# else
  set_ids();
# endif
  EdgeMask used(edges_.size());
  for (unsigned i = 0; i < vit_edges.size(); ++i)
    used[vit_edges[i]->id_]=true;
  return CreateEdgeSubset(used);
#else
  map<int, int> old2new_node;
  int num_new_nodes = 0;
  for (unsigned i = 0; i < vit_edges.size(); ++i) {
    const Edge& edge = *vit_edges[i];
    for (unsigned j = 0; j < edge.tail_nodes_.size(); ++j) assert(old2new_node.count(edge.tail_nodes_[j]) > 0);
    if (old2new_node.count(edge.head_node_) == 0) {
      old2new_node[edge.head_node_] = num_new_nodes;
      ++num_new_nodes;
    }
  }
  HypergraphP ret(new Hypergraph(num_new_nodes, vit_edges.size(), is_linear_chain_));
  Hypergraph* out = ret.get();
  for (map<int, int>::iterator it = old2new_node.begin();
       it != old2new_node.end(); ++it) {
    const Node& old_node = nodes_[it->first];
    Node& new_node = out->nodes_[it->second];
    new_node.cat_ = old_node.cat_;
    new_node.id_ = it->second;
  }

  for (unsigned i = 0; i < vit_edges.size(); ++i) {
    const Edge& old_edge = *vit_edges[i];
    Edge& new_edge = out->edges_[i];
    new_edge = old_edge;
    new_edge.id_ = i;
    const int new_head_node = old2new_node[old_edge.head_node_];
    new_edge.head_node_ = new_head_node;
    out->nodes_[new_head_node].in_edges_.push_back(i);
    for (unsigned j = 0; j < old_edge.tail_nodes_.size(); ++j) {
      const int new_tail_node = old2new_node[old_edge.tail_nodes_[j]];
      new_edge.tail_nodes_[j] = new_tail_node;
      out->nodes_[new_tail_node].out_edges_.push_back(i);
    }
  }
  return ret;
#endif
}

