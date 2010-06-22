#include "viterbi_envelope.h"

#include <cassert>
#include <limits>

using namespace std;
using boost::shared_ptr;

ostream& operator<<(ostream& os, const ViterbiEnvelope& env) {
  os << '<';
  const vector<shared_ptr<Segment> >& segs = env.GetSortedSegs();
  for (int i = 0; i < segs.size(); ++i)
    os << (i==0 ? "" : "|") << "x=" << segs[i]->x << ",b=" << segs[i]->b << ",m=" << segs[i]->m << ",p1=" << segs[i]->p1 << ",p2=" << segs[i]->p2;
  return os << '>';
}

ViterbiEnvelope::ViterbiEnvelope(int i) {
  if (i == 0) {
    // do nothing - <>
  } else if (i == 1) {
    segs.push_back(shared_ptr<Segment>(new Segment(0, 0, 0, shared_ptr<Segment>(), shared_ptr<Segment>())));
    assert(this->IsMultiplicativeIdentity());
  } else {
    cerr << "Only can create ViterbiEnvelope semiring 0 and 1 with this constructor!\n";
    abort();
  }
}

struct SlopeCompare {
  bool operator() (const shared_ptr<Segment>& a, const shared_ptr<Segment>& b) const {
    return a->m < b->m;
  }
};

const ViterbiEnvelope& ViterbiEnvelope::operator+=(const ViterbiEnvelope& other) {
  if (!other.is_sorted) other.Sort();
  if (segs.empty()) {
    segs = other.segs;
    return *this;
  }
  is_sorted = false;
  int j = segs.size();
  segs.resize(segs.size() + other.segs.size());
  for (int i = 0; i < other.segs.size(); ++i)
    segs[j++] = other.segs[i];
  assert(j == segs.size());
  return *this;
}

void ViterbiEnvelope::Sort() const {
  sort(segs.begin(), segs.end(), SlopeCompare());
  const int k = segs.size();
  int j = 0;
  for (int i = 0; i < k; ++i) {
    Segment l = *segs[i];
    l.x = kMinusInfinity;
    // cerr << "m=" << l.m << endl;
    if (0 < j) {
      if (segs[j-1]->m == l.m) {   // lines are parallel
        if (l.b <= segs[j-1]->b) continue;
        --j;
      }
      while(0 < j) {
        l.x = (l.b - segs[j-1]->b) / (segs[j-1]->m - l.m);
        if (segs[j-1]->x < l.x) break;
        --j;
      }
      if (0 == j) l.x = kMinusInfinity;
    }
    *segs[j++] = l;
  }
  segs.resize(j);
  is_sorted = true;
}

const ViterbiEnvelope& ViterbiEnvelope::operator*=(const ViterbiEnvelope& other) {
  if (other.IsMultiplicativeIdentity()) { return *this; }
  if (this->IsMultiplicativeIdentity()) { (*this) = other; return *this; }

  if (!is_sorted) Sort();
  if (!other.is_sorted) other.Sort();

  if (this->IsEdgeEnvelope()) {
//    if (other.size() > 1)
//      cerr << *this << " (TIMES) " << other << endl;
    shared_ptr<Segment> edge_parent = segs[0];
    const double& edge_b = edge_parent->b;
    const double& edge_m = edge_parent->m;
    segs.clear();
    for (int i = 0; i < other.segs.size(); ++i) {
      const Segment& seg = *other.segs[i];
      const double m = seg.m + edge_m;
      const double b = seg.b + edge_b;
      const double& x = seg.x;       // x's don't change with *
      segs.push_back(shared_ptr<Segment>(new Segment(x, m, b, edge_parent, other.segs[i])));
      assert(segs.back()->p1->edge);
    }
//    if (other.size() > 1)
//      cerr << " = " << *this << endl;
  } else {
    vector<shared_ptr<Segment> > new_segs;
    int this_i = 0;
    int other_i = 0;
    const int this_size  = segs.size();
    const int other_size = other.segs.size();
    double cur_x = kMinusInfinity;   // moves from left to right across the
                                     // real numbers, stopping for all inter-
                                     // sections
    double this_next_val  = (1 < this_size  ? segs[1]->x       : kPlusInfinity);
    double other_next_val = (1 < other_size ? other.segs[1]->x : kPlusInfinity);
    while (this_i < this_size && other_i < other_size) {
      const Segment& this_seg = *segs[this_i];
      const Segment& other_seg= *other.segs[other_i];
      const double m = this_seg.m + other_seg.m;
      const double b = this_seg.b + other_seg.b;
 
      new_segs.push_back(shared_ptr<Segment>(new Segment(cur_x, m, b, segs[this_i], other.segs[other_i])));
      int comp = 0;
      if (this_next_val < other_next_val) comp = -1; else
        if (this_next_val > other_next_val) comp = 1;
      if (0 == comp) {  // the next values are equal, advance both indices
        ++this_i;
	++other_i;
        cur_x = this_next_val;  // could be other_next_val (they're equal!)
        this_next_val  = (this_i+1  < this_size  ? segs[this_i+1]->x        : kPlusInfinity);
        other_next_val = (other_i+1 < other_size ? other.segs[other_i+1]->x : kPlusInfinity);
      } else {  // advance the i with the lower x, update cur_x
        if (-1 == comp) {
          ++this_i;
          cur_x = this_next_val;
          this_next_val =  (this_i+1  < this_size  ? segs[this_i+1]->x        : kPlusInfinity);
        } else {
          ++other_i;
          cur_x = other_next_val;
          other_next_val = (other_i+1 < other_size ? other.segs[other_i+1]->x : kPlusInfinity);
        }
      }
    }
    segs.swap(new_segs);
  }
  //cerr << "Multiply: result=" << (*this) << endl;
  return *this;
}

// recursively construct translation
void Segment::ConstructTranslation(vector<WordID>* trans) const {
  const Segment* cur = this;
  vector<vector<WordID> > ant_trans;
  while(!cur->edge) {
    ant_trans.resize(ant_trans.size() + 1);
    cur->p2->ConstructTranslation(&ant_trans.back());
    cur = cur->p1.get();
  }
  size_t ant_size = ant_trans.size();
  vector<const vector<WordID>*> pants(ant_size);
  --ant_size;
  for (int i = 0; i < pants.size(); ++i) pants[ant_size - i] = &ant_trans[i];
  cur->edge->rule_->ESubstitute(pants, trans);
}

void Segment::CollectEdgesUsed(std::vector<bool>* edges_used) const {
  if (edge) {
    assert(edge->id_ < edges_used->size());
    (*edges_used)[edge->id_] = true;
  }
  if (p1) p1->CollectEdgesUsed(edges_used);
  if (p2) p2->CollectEdgesUsed(edges_used);
}

ViterbiEnvelope ViterbiEnvelopeWeightFunction::operator()(const Hypergraph::Edge& e) const {
  const double m = direction.dot(e.feature_values_);
  const double b = origin.dot(e.feature_values_);
  Segment* seg = new Segment(m, b, e);
  return ViterbiEnvelope(1, seg);
}

