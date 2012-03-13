#ifndef _CES_H_
#define _CES_H_

class ConvexHull;
class Hypergraph;
class SegmentEvaluator;
class ErrorSurface;
class EvaluationMetric;

void ComputeErrorSurface(const SegmentEvaluator& ss,
                         const ConvexHull& convex_hull,
                         ErrorSurface* es,
                         const EvaluationMetric* metric,
                         const Hypergraph& hg);

#endif
