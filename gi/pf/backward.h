#ifndef _BACKWARD_H_
#define _BACKWARD_H_

#include <vector>
#include <string>
#include "wordid.h"

struct Reachability;
struct Model1;

struct BackwardEstimator {
  BackwardEstimator(const std::string& s2t,
                    const std::string& t2s);
  ~BackwardEstimator();

  void InitializeGrid(const std::vector<WordID>& src,
                      const std::vector<WordID>& trg,
                      const Reachability& r,
                      double src2trg_ratio,
                      float* grid) const;

 private:
  float ComputeBackwardProb(const std::vector<WordID>& src,
                            const std::vector<WordID>& trg,
                            unsigned src_covered,
                            unsigned trg_covered,
                            double src2trg_ratio) const;

  Model1* m1;
  Model1* m1inv;
};

#endif
