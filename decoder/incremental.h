#ifndef _INCREMENTAL_H_
#define _INCREMENTAL_H_

#include "weights.h"
#include <vector>

class Hypergraph;

class IncrementalBase {
  public:
    static IncrementalBase *Load(const char *model_file, const std::vector<weight_t> &weights);

    virtual ~IncrementalBase();

    virtual void Search(unsigned int pop_limit, const Hypergraph &hg) const = 0;

  protected:
    IncrementalBase(const std::vector<weight_t> &weights);

    const std::vector<weight_t> &cdec_weights_;
};

#endif // _INCREMENTAL_H_
