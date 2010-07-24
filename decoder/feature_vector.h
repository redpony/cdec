#ifndef _FEATURE_VECTOR_H_
#define _FEATURE_VECTOR_H_

#include <vector>
#include "sparse_vector.h"
#include "fdict.h"

typedef double Featval;
typedef SparseVectorList<Featval> FeatureVectorList;
typedef SparseVector<Featval> FeatureVector;
typedef SparseVector<Featval> WeightVector;
typedef std::vector<Featval> DenseWeightVector;

#endif
