#ifndef _CES_H_
#define _CES_H_

class ViterbiEnvelope;
class Hypergraph;
class SegmentEvaluator;
class ErrorSurface;
class EvaluationMetric;

void ComputeErrorSurface(const SegmentEvaluator& ss,
                         const ViterbiEnvelope& ve,
                         ErrorSurface* es,
                         const EvaluationMetric* metric,
                         const Hypergraph& hg);

#endif
