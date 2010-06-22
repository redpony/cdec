#ifndef _VITERBI_ENVELOPE_H_
#define _VITERBI_ENVELOPE_H_

#include <vector>
#include <iostream>
#include <boost/shared_ptr.hpp>

#include "hg.h"
#include "sparse_vector.h"

static const double kMinusInfinity = -std::numeric_limits<double>::infinity();
static const double kPlusInfinity = std::numeric_limits<double>::infinity();

struct Segment {
  Segment() : x(), m(), b(), edge() {}
  Segment(double _m, double _b) :
    x(kMinusInfinity), m(_m), b(_b), edge() {}
  Segment(double _x, double _m, double _b, const boost::shared_ptr<Segment>& p1_, const boost::shared_ptr<Segment>& p2_) :
    x(_x), m(_m), b(_b), p1(p1_), p2(p2_), edge() {}
  Segment(double _m, double _b, const Hypergraph::Edge& edge) :
    x(kMinusInfinity), m(_m), b(_b), edge(&edge) {}

  double x;                   // x intersection with previous segment in env, or -inf if none
  double m;                   // this line's slope
  double b;                   // intercept with y-axis

  // we keep a pointer to the "parents" of this segment so we can reconstruct
  // the Viterbi translation corresponding to this segment
  boost::shared_ptr<Segment> p1;
  boost::shared_ptr<Segment> p2;

  // only Segments created from an edge using the ViterbiEnvelopeWeightFunction
  // have rules
  // TRulePtr rule;
  const Hypergraph::Edge* edge;

  // recursively recover the Viterbi translation that will result from setting
  // the weights to origin + axis * x, where x is any value from this->x up
  // until the next largest x in the containing ViterbiEnvelope
  void ConstructTranslation(std::vector<WordID>* trans) const;
  void CollectEdgesUsed(std::vector<bool>* edges_used) const;
};

// this is the semiring value type,
// it defines constructors for 0, 1, and the operations + and *
struct ViterbiEnvelope {
  // create semiring zero
  ViterbiEnvelope() : is_sorted(true) {}  // zero
  // for debugging:
  ViterbiEnvelope(const std::vector<boost::shared_ptr<Segment> >& s) : segs(s) { Sort(); }
  // create semiring 1 or 0
  explicit ViterbiEnvelope(int i);
  ViterbiEnvelope(int n, Segment* seg) : is_sorted(true), segs(n, boost::shared_ptr<Segment>(seg)) {}
  const ViterbiEnvelope& operator+=(const ViterbiEnvelope& other);
  const ViterbiEnvelope& operator*=(const ViterbiEnvelope& other);
  bool IsMultiplicativeIdentity() const {
    return size() == 1 && (segs[0]->b == 0.0 && segs[0]->m == 0.0) && (!segs[0]->edge); }
  const std::vector<boost::shared_ptr<Segment> >& GetSortedSegs() const {
    if (!is_sorted) Sort();
    return segs;
  }
  size_t size() const { return segs.size(); }

 private:
  bool IsEdgeEnvelope() const {
    return segs.size() == 1 && segs[0]->edge; }
  void Sort() const;
  mutable bool is_sorted;
  mutable std::vector<boost::shared_ptr<Segment> > segs;
};
std::ostream& operator<<(std::ostream& os, const ViterbiEnvelope& env);

struct ViterbiEnvelopeWeightFunction {
  ViterbiEnvelopeWeightFunction(const SparseVector<double>& ori,
                                const SparseVector<double>& dir) : origin(ori), direction(dir) {}
  ViterbiEnvelope operator()(const Hypergraph::Edge& e) const;
  const SparseVector<double> origin;
  const SparseVector<double> direction;
};

#endif
