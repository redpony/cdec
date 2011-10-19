#ifndef _FEATURE_VECTOR_H_
#define _FEATURE_VECTOR_H_

#include <vector>
#include "sparse_vector.h"
#include "weights.h"

typedef weight_t Featval;
typedef SparseVector<Featval> FeatureVector;
typedef SparseVector<Featval> WeightVector;
typedef std::vector<Featval> DenseWeightVector;

inline void sparse_to_dense(WeightVector const& wv,DenseWeightVector *dv) {
  wv.init_vector(dv);
}

#endif
