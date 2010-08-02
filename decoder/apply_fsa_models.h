#ifndef _APPLY_FSA_MODELS_H_
#define _APPLY_FSA_MODELS_H_

//#include "ff_fsa_dynamic.h"

struct FsaFeatureFunction;
struct Hypergraph;
struct SentenceMetadata;

void ApplyFsaModels(const Hypergraph& in,
                    const SentenceMetadata& smeta,
                    const FsaFeatureFunction& fsa,
                    Hypergraph* out);

#endif
