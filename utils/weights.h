#ifndef _WEIGHTS_H_
#define _WEIGHTS_H_

#include <string>
#include <vector>
#include "sparse_vector.h"

// warning: in the future this will become float
typedef double weight_t;

class Weights {
 public:
  static void InitFromFile(const std::string& fname,
                           std::vector<weight_t>* weights,
                           std::vector<std::string>* feature_list = NULL);
  static void WriteToFile(const std::string& fname,
                          const std::vector<weight_t>& weights,
                          bool hide_zero_value_features = true,
                          const std::string* extra = NULL);
  static void InitSparseVector(const std::vector<weight_t>& dv,
                               SparseVector<weight_t>* sv);
  // check for infinities, NaNs, etc
  static void SanityCheck(const std::vector<weight_t>& w);
  // write weights with largest magnitude to cerr
  static void ShowLargestFeatures(const std::vector<weight_t>& w);
  static std::string GetString(const std::vector<weight_t>& w,
                               bool hide_zero_value_features = true);
  // Assumes weights are already initialized for now
  static void UpdateFromString(std::string& w_string,
                               std::vector<weight_t>& w);
 private:
  Weights();
};

#endif
