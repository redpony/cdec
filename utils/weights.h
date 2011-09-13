#ifndef _WEIGHTS_H_
#define _WEIGHTS_H_

#include <string>
#include <vector>
#include "sparse_vector.h"

// warning: in the future this will become float
typedef double weight_t;

class Weights {
 public:
  Weights() {}
  void InitFromFile(const std::string& fname, std::vector<std::string>* feature_list = NULL);
  void WriteToFile(const std::string& fname, bool hide_zero_value_features = true, const std::string* extra = NULL) const;
  void InitVector(std::vector<weight_t>* w) const;
  void InitSparseVector(SparseVector<weight_t>* w) const;
  void InitFromVector(const std::vector<weight_t>& w);
  void InitFromVector(const SparseVector<weight_t>& w);
 private:
  std::vector<weight_t> wv_;
};

#endif
