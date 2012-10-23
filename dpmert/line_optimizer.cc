#include "line_optimizer.h"

#include <limits>
#include <algorithm>

#include "sparse_vector.h"
#include "ns.h"

using namespace std;

typedef ErrorSurface::const_iterator ErrorIter;

// sort by increasing x-ints
struct IntervalComp {
  bool operator() (const ErrorIter& a, const ErrorIter& b) const {
    return a->x < b->x;
  }
};

double LineOptimizer::LineOptimize(
    const EvaluationMetric* metric,
    const vector<ErrorSurface>& surfaces,
    const LineOptimizer::ScoreType type,
    float* best_score,
    const double epsilon) {
  // cerr << "MIN=" << MINIMIZE_SCORE << " MAX=" << MAXIMIZE_SCORE << "  MINE=" << type << endl;
  vector<ErrorIter> all_ints;
  for (vector<ErrorSurface>::const_iterator i = surfaces.begin();
       i != surfaces.end(); ++i) {
    const ErrorSurface& surface = *i;
    for (ErrorIter j = surface.begin(); j != surface.end(); ++j)
      all_ints.push_back(j);
  }
  sort(all_ints.begin(), all_ints.end(), IntervalComp());
  double last_boundary = all_ints.front()->x;
  SufficientStats acc;
  float& cur_best_score = *best_score;
  cur_best_score = (type == MAXIMIZE_SCORE ?
    -numeric_limits<float>::max() : numeric_limits<float>::max());
  bool left_edge = true;
  double pos = numeric_limits<double>::quiet_NaN();
  for (vector<ErrorIter>::iterator i = all_ints.begin();
       i != all_ints.end(); ++i) {
    const ErrorSegment& seg = **i;
    if (seg.x - last_boundary > epsilon) {
      float sco = metric->ComputeScore(acc);
      if ((type == MAXIMIZE_SCORE && sco > cur_best_score) ||
          (type == MINIMIZE_SCORE && sco < cur_best_score) ) {
        cur_best_score = sco;
	if (left_edge) {
	  pos = seg.x - 0.1;
	  left_edge = false;
	} else {
	  pos = last_boundary + (seg.x - last_boundary) / 2;
	}
	//cerr << "NEW BEST: " << pos << "  (score=" << cur_best_score << ")\n";
      }
      // string xx = metric->DetailedScore(acc); cerr << "---- " << xx;
#undef SHOW_ERROR_SURFACES
#ifdef SHOW_ERROR_SURFACES
      cerr << "x=" << seg.x << "\ts=" << sco << "\n";
#endif
      last_boundary = seg.x;
    }
    // cerr << "x-boundary=" << seg.x << "\n";
    //string x2; acc.Encode(&x2); cerr << "   ACC: " << x2 << endl;
    //string x1; seg.delta.Encode(&x1); cerr << " DELTA: " << x1 << endl;
    acc += seg.delta;
  }
  float sco = metric->ComputeScore(acc);
  if ((type == MAXIMIZE_SCORE && sco > cur_best_score) ||
      (type == MINIMIZE_SCORE && sco < cur_best_score) ) {
    cur_best_score = sco;
    if (left_edge) {
      pos = 0;
    } else {
      pos = last_boundary + 1000.0;
    }
  }
  return pos;
}

void LineOptimizer::RandomUnitVector(const vector<int>& features_to_optimize,
                                     SparseVector<double>* axis,
                                     RandomNumberGenerator<boost::mt19937>* rng) {
  axis->clear();
  for (int i = 0; i < features_to_optimize.size(); ++i)
    axis->set_value(features_to_optimize[i], rng->NextNormal(0.0,1.0));
  (*axis) /= axis->l2norm();
}

void LineOptimizer::CreateOptimizationDirections(
     const vector<int>& features_to_optimize,
     int additional_random_directions,
     RandomNumberGenerator<boost::mt19937>* rng,
     vector<SparseVector<double> >* dirs
     , bool include_orthogonal
  ) {
  dirs->clear();
  typedef SparseVector<double> Dir;
  vector<Dir> &out=*dirs;
  int i=0;
  if (include_orthogonal)
    for (;i<features_to_optimize.size();++i) {
      Dir d;
      d.set_value(features_to_optimize[i],1.);
      out.push_back(d);
    }
  out.resize(i+additional_random_directions);
  for (;i<out.size();++i)
     RandomUnitVector(features_to_optimize, &out[i], rng);
  cerr << "Generated " << out.size() << " total axes to optimize along.\n";
}

