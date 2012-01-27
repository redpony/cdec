#ifndef _MERT_GEOMETRY_H_
#define _MERT_GEOMETRY_H_

#include <vector>
#include <iostream>
#include <boost/shared_ptr.hpp>

#include "hg.h"
#include "sparse_vector.h"

static const double kMinusInfinity = -std::numeric_limits<double>::infinity();
static const double kPlusInfinity = std::numeric_limits<double>::infinity();

struct MERTPoint {
  MERTPoint() : x(), m(), b(), edge() {}
  MERTPoint(double _m, double _b) :
    x(kMinusInfinity), m(_m), b(_b), edge() {}
  MERTPoint(double _x, double _m, double _b, const boost::shared_ptr<MERTPoint>& p1_, const boost::shared_ptr<MERTPoint>& p2_) :
    x(_x), m(_m), b(_b), p1(p1_), p2(p2_), edge() {}
  MERTPoint(double _m, double _b, const Hypergraph::Edge& edge) :
    x(kMinusInfinity), m(_m), b(_b), edge(&edge) {}

  double x;                   // x intersection with previous segment in env, or -inf if none
  double m;                   // this line's slope
  double b;                   // intercept with y-axis

  // we keep a pointer to the "parents" of this segment so we can reconstruct
  // the Viterbi translation corresponding to this segment
  boost::shared_ptr<MERTPoint> p1;
  boost::shared_ptr<MERTPoint> p2;

  // only MERTPoints created from an edge using the ConvexHullWeightFunction
  // have rules
  // TRulePtr rule;
  const Hypergraph::Edge* edge;

  // recursively recover the Viterbi translation that will result from setting
  // the weights to origin + axis * x, where x is any value from this->x up
  // until the next largest x in the containing ConvexHull
  void ConstructTranslation(std::vector<WordID>* trans) const;
  void CollectEdgesUsed(std::vector<bool>* edges_used) const;
};

// this is the semiring value type,
// it defines constructors for 0, 1, and the operations + and *
struct ConvexHull {
  // create semiring zero
  ConvexHull() : is_sorted(true) {}  // zero
  // for debugging:
  ConvexHull(const std::vector<boost::shared_ptr<MERTPoint> >& s) : points(s) { Sort(); }
  // create semiring 1 or 0
  explicit ConvexHull(int i);
  ConvexHull(int n, MERTPoint* point) : is_sorted(true), points(n, boost::shared_ptr<MERTPoint>(point)) {}
  const ConvexHull& operator+=(const ConvexHull& other);
  const ConvexHull& operator*=(const ConvexHull& other);
  bool IsMultiplicativeIdentity() const {
    return size() == 1 && (points[0]->b == 0.0 && points[0]->m == 0.0) && (!points[0]->edge) && (!points[0]->p1) && (!points[0]->p2); }
  const std::vector<boost::shared_ptr<MERTPoint> >& GetSortedSegs() const {
    if (!is_sorted) Sort();
    return points;
  }
  size_t size() const { return points.size(); }

 private:
  bool IsEdgeEnvelope() const {
    return points.size() == 1 && points[0]->edge; }
  void Sort() const;
  mutable bool is_sorted;
  mutable std::vector<boost::shared_ptr<MERTPoint> > points;
};
std::ostream& operator<<(std::ostream& os, const ConvexHull& env);

struct ConvexHullWeightFunction {
  ConvexHullWeightFunction(const SparseVector<double>& ori,
                           const SparseVector<double>& dir) : origin(ori), direction(dir) {}
  const ConvexHull operator()(const Hypergraph::Edge& e) const;
  const SparseVector<double> origin;
  const SparseVector<double> direction;
};

#endif
