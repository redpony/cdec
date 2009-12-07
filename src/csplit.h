#ifndef _CSPLIT_H_
#define _CSPLIT_H_

#include "translator.h"
#include "lattice.h"

struct CompoundSplitImpl;
struct CompoundSplit : public Translator {
  CompoundSplit(const boost::program_options::variables_map& conf);
  bool Translate(const std::string& input,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* forest);
 private:
  boost::shared_ptr<CompoundSplitImpl> pimpl_;
};

#endif
