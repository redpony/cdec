#ifndef _DEP_TRAINING_H_
#define _DEP_TRAINING_H_

#include <iostream>
#include <string>
#include <vector>
#include "arc_factored.h"
#include "weights.h"

struct TrainingInstance {
  TaggedSentence ts;
  EdgeSubset tree;
  SparseVector<weight_t> features;
  // reads a "Jsent" formatted dependency file
  static bool ReadInstance(std::istream* in, TrainingInstance* instance); // returns false at EOF
  static void ReadTrainingCorpus(const std::string& fname, std::vector<TrainingInstance>* corpus, int rank = 0, int size = 1);
};

#endif
