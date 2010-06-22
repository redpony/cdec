#include "line_optimizer.h"

#include <limits>
#include <algorithm>

#include "sparse_vector.h"
#include "scorer.h"

using namespace std;

typedef ErrorSurface::const_iterator ErrorIter;

// sort by increasing x-ints
struct IntervalComp {
  bool operator() (const ErrorIter& a, const ErrorIter& b) const {
    return a->x < b->x;
  }
};	

double LineOptimizer::LineOptimize(
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
  Score* acc = all_ints.front()->delta->GetZero();
  float& cur_best_score = *best_score;
  cur_best_score = (type == MAXIMIZE_SCORE ?
    -numeric_limits<float>::max() : numeric_limits<float>::max());
  bool left_edge = true;
  double pos = numeric_limits<double>::quiet_NaN();
  for (vector<ErrorIter>::iterator i = all_ints.begin();
       i != all_ints.end(); ++i) {
    const ErrorSegment& seg = **i;
    assert(seg.delta);
    if (seg.x - last_boundary > epsilon) {
      float sco = acc->ComputeScore();
      if ((type == MAXIMIZE_SCORE && sco > cur_best_score) ||
          (type == MINIMIZE_SCORE && sco < cur_best_score) ) {
        cur_best_score = sco;
	if (left_edge) {
	  pos = seg.x - 0.1;
	  left_edge = false;
	} else {
	  pos = last_boundary + (seg.x - last_boundary) / 2;
	}
	// cerr << "NEW BEST: " << pos << "  (score=" << cur_best_score << ")\n";
      }
      // string xx; acc->ScoreDetails(&xx); cerr << "---- " << xx;
      // cerr << "---- s=" << sco << "\n";
      last_boundary = seg.x;
    }
    // cerr << "x-boundary=" << seg.x << "\n";
    acc->PlusEquals(*seg.delta);
  }
  float sco = acc->ComputeScore();
  if ((type == MAXIMIZE_SCORE && sco > cur_best_score) ||
      (type == MINIMIZE_SCORE && sco < cur_best_score) ) {
    cur_best_score = sco;
    if (left_edge) {
      pos = 0;
    } else {
      pos = last_boundary + 1000.0;
    }
  }
  delete acc;
  return pos;
}

void LineOptimizer::RandomUnitVector(const vector<int>& features_to_optimize,
                                     SparseVector<double>* axis,
                                     RandomNumberGenerator<boost::mt19937>* rng) {
  axis->clear();
  for (int i = 0; i < features_to_optimize.size(); ++i)
    axis->set_value(features_to_optimize[i], rng->next() - 0.5);
  (*axis) /= axis->l2norm();
}

void LineOptimizer::CreateOptimizationDirections(
     const vector<int>& features_to_optimize,
     int additional_random_directions,
     RandomNumberGenerator<boost::mt19937>* rng,
     vector<SparseVector<double> >* dirs) {
  const int num_directions = features_to_optimize.size() + additional_random_directions;
  dirs->resize(num_directions);
  for (int i = 0; i < num_directions; ++i) {
    SparseVector<double>& axis = (*dirs)[i];
    if (i < features_to_optimize.size())
      axis.set_value(features_to_optimize[i], 1.0);
    else
      RandomUnitVector(features_to_optimize, &axis, rng);
  }
  cerr << "Generated " << num_directions << " total axes to optimize along.\n";
}
