#include "mert_geometry.h"

#include <cassert>
#include <limits>

using namespace std;

ConvexHull::ConvexHull(int i) {
  if (i == 0) {
    // do nothing - <>
  } else if (i == 1) {
    points.push_back(boost::shared_ptr<MERTPoint>(new MERTPoint(0, 0, 0, boost::shared_ptr<MERTPoint>(), boost::shared_ptr<MERTPoint>())));
    assert(this->IsMultiplicativeIdentity());
  } else {
    cerr << "Only can create ConvexHull semiring 0 and 1 with this constructor!\n";
    abort();
  }
}

const ConvexHull ConvexHullWeightFunction::operator()(const Hypergraph::Edge& e) const {
  const double m = direction.dot(e.feature_values_);
  const double b = origin.dot(e.feature_values_);
  MERTPoint* point = new MERTPoint(m, b, e);
  return ConvexHull(1, point);
}

ostream& operator<<(ostream& os, const ConvexHull& env) {
  os << '<';
  const vector<boost::shared_ptr<MERTPoint> >& points = env.GetSortedSegs();
  for (int i = 0; i < points.size(); ++i)
    os << (i==0 ? "" : "|") << "x=" << points[i]->x << ",b=" << points[i]->b << ",m=" << points[i]->m << ",p1=" << points[i]->p1 << ",p2=" << points[i]->p2;
  return os << '>';
}

#define ORIGINAL_MERT_IMPLEMENTATION 1
#ifdef ORIGINAL_MERT_IMPLEMENTATION

struct SlopeCompare {
  bool operator() (const boost::shared_ptr<MERTPoint>& a, const boost::shared_ptr<MERTPoint>& b) const {
    return a->m < b->m;
  }
};

const ConvexHull& ConvexHull::operator+=(const ConvexHull& other) {
  if (!other.is_sorted) other.Sort();
  if (points.empty()) {
    points = other.points;
    return *this;
  }
  is_sorted = false;
  int j = points.size();
  points.resize(points.size() + other.points.size());
  for (int i = 0; i < other.points.size(); ++i)
    points[j++] = other.points[i];
  assert(j == points.size());
  return *this;
}

void ConvexHull::Sort() const {
  sort(points.begin(), points.end(), SlopeCompare());
  const int k = points.size();
  int j = 0;
  for (int i = 0; i < k; ++i) {
    MERTPoint l = *points[i];
    l.x = kMinusInfinity;
    // cerr << "m=" << l.m << endl;
    if (0 < j) {
      if (points[j-1]->m == l.m) {   // lines are parallel
        if (l.b <= points[j-1]->b) continue;
        --j;
      }
      while(0 < j) {
        l.x = (l.b - points[j-1]->b) / (points[j-1]->m - l.m);
        if (points[j-1]->x < l.x) break;
        --j;
      }
      if (0 == j) l.x = kMinusInfinity;
    }
    *points[j++] = l;
  }
  points.resize(j);
  is_sorted = true;
}

const ConvexHull& ConvexHull::operator*=(const ConvexHull& other) {
  if (other.IsMultiplicativeIdentity()) { return *this; }
  if (this->IsMultiplicativeIdentity()) { (*this) = other; return *this; }

  if (!is_sorted) Sort();
  if (!other.is_sorted) other.Sort();

  if (this->IsEdgeEnvelope()) {
//    if (other.size() > 1)
//      cerr << *this << " (TIMES) " << other << endl;
    boost::shared_ptr<MERTPoint> edge_parent = points[0];
    const double& edge_b = edge_parent->b;
    const double& edge_m = edge_parent->m;
    points.clear();
    for (int i = 0; i < other.points.size(); ++i) {
      const MERTPoint& p = *other.points[i];
      const double m = p.m + edge_m;
      const double b = p.b + edge_b;
      const double& x = p.x;       // x's don't change with *
      points.push_back(boost::shared_ptr<MERTPoint>(new MERTPoint(x, m, b, edge_parent, other.points[i])));
      assert(points.back()->p1->edge);
    }
//    if (other.size() > 1)
//      cerr << " = " << *this << endl;
  } else {
    vector<boost::shared_ptr<MERTPoint> > new_points;
    int this_i = 0;
    int other_i = 0;
    const int this_size  = points.size();
    const int other_size = other.points.size();
    double cur_x = kMinusInfinity;   // moves from left to right across the
                                     // real numbers, stopping for all inter-
                                     // sections
    double this_next_val  = (1 < this_size  ? points[1]->x       : kPlusInfinity);
    double other_next_val = (1 < other_size ? other.points[1]->x : kPlusInfinity);
    while (this_i < this_size && other_i < other_size) {
      const MERTPoint& this_point = *points[this_i];
      const MERTPoint& other_point= *other.points[other_i];
      const double m = this_point.m + other_point.m;
      const double b = this_point.b + other_point.b;
 
      new_points.push_back(boost::shared_ptr<MERTPoint>(new MERTPoint(cur_x, m, b, points[this_i], other.points[other_i])));
      int comp = 0;
      if (this_next_val < other_next_val) comp = -1; else
        if (this_next_val > other_next_val) comp = 1;
      if (0 == comp) {  // the next values are equal, advance both indices
        ++this_i;
	++other_i;
        cur_x = this_next_val;  // could be other_next_val (they're equal!)
        this_next_val  = (this_i+1  < this_size  ? points[this_i+1]->x        : kPlusInfinity);
        other_next_val = (other_i+1 < other_size ? other.points[other_i+1]->x : kPlusInfinity);
      } else {  // advance the i with the lower x, update cur_x
        if (-1 == comp) {
          ++this_i;
          cur_x = this_next_val;
          this_next_val =  (this_i+1  < this_size  ? points[this_i+1]->x        : kPlusInfinity);
        } else {
          ++other_i;
          cur_x = other_next_val;
          other_next_val = (other_i+1 < other_size ? other.points[other_i+1]->x : kPlusInfinity);
        }
      }
    }
    points.swap(new_points);
  }
  //cerr << "Multiply: result=" << (*this) << endl;
  return *this;
}

// recursively construct translation
void MERTPoint::ConstructTranslation(vector<WordID>* trans) const {
  const MERTPoint* cur = this;
  vector<vector<WordID> > ant_trans;
  while(!cur->edge) {
    ant_trans.resize(ant_trans.size() + 1);
    cur->p2->ConstructTranslation(&ant_trans.back());
    cur = cur->p1.get();
  }
  size_t ant_size = ant_trans.size();
  vector<const vector<WordID>*> pants(ant_size);
  assert(ant_size == cur->edge->tail_nodes_.size());
  --ant_size;
  for (int i = 0; i < pants.size(); ++i) pants[ant_size - i] = &ant_trans[i];
  cur->edge->rule_->ESubstitute(pants, trans);
}

void MERTPoint::CollectEdgesUsed(std::vector<bool>* edges_used) const {
  if (edge) {
    assert(edge->id_ < edges_used->size());
    (*edges_used)[edge->id_] = true;
  }
  if (p1) p1->CollectEdgesUsed(edges_used);
  if (p2) p2->CollectEdgesUsed(edges_used);
}

#else

// THIS IS THE NEW FASTER IMPLEMENTATION OF THE MERT SEMIRING OPERATIONS

#endif

