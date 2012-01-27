#ifndef LINE_OPTIMIZER_H_
#define LINE_OPTIMIZER_H_

#include <vector>

#include "sparse_vector.h"
#include "error_surface.h"
#include "sampler.h"

class EvaluationMetric;
class Weights;

struct LineOptimizer {

  // use MINIMIZE_SCORE for things like TER, WER
  // MAXIMIZE_SCORE for things like BLEU
  enum ScoreType { MAXIMIZE_SCORE, MINIMIZE_SCORE };

  // merge all the error surfaces together into a global
  // error surface and find (the middle of) the best segment
  static double LineOptimize(
     const EvaluationMetric* metric,
     const std::vector<ErrorSurface>& envs,
     const LineOptimizer::ScoreType type,
     float* best_score,
     const double epsilon = 1.0/65536.0);

  // return a random vector of length 1 where all dimensions
  // not listed in dimensions will be 0.
  static void RandomUnitVector(const std::vector<int>& dimensions,
                               SparseVector<double>* axis,
                               RandomNumberGenerator<boost::mt19937>* rng);

  // generate a list of directions to optimize; the list will
  // contain the orthogonal vectors corresponding to the dimensions in
  // primary and then additional_random_directions directions in those
  // dimensions as well.  All vectors will be length 1.
  static void CreateOptimizationDirections(
     const std::vector<int>& primary,
     int additional_random_directions,
     RandomNumberGenerator<boost::mt19937>* rng,
     std::vector<SparseVector<double> >* dirs
     , bool include_primary=true
    );

};

#endif
