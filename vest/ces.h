#ifndef _CES_H_
#define _CES_H_

#include "scorer.h"

class ViterbiEnvelope;
class Hypergraph;
class ErrorSurface;

void ComputeErrorSurface(const SentenceScorer& ss, const ViterbiEnvelope& ve, ErrorSurface* es, const ScoreType type, const Hypergraph& hg);

#endif
