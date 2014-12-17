#ifndef TRAINING_CONST_REORDER_TRAINER_H_
#define TRAINING_CONST_REORDER_TRAINER_H_

#include "decoder/ff_const_reorder_common.h"

struct Tsuruoka_Maxent_Trainer : const_reorder::Tsuruoka_Maxent {
  Tsuruoka_Maxent_Trainer();
  void fnTrain(const char* pszInstanceFName, const char* pszAlgorithm,
               const char* pszModelFName);
};

#endif  // TRAINING_CONST_REORDER_TRAINER_H_
