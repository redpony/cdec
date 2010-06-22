#ifndef _LEXTrans_H_
#define _LEXTrans_H_

#include "translator.h"
#include "lattice.h"

struct LexicalTransImpl;
struct LexicalTrans : public Translator {
  LexicalTrans(const boost::program_options::variables_map& conf);
  bool TranslateImpl(const std::string& input,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* forest);
 private:
  boost::shared_ptr<LexicalTransImpl> pimpl_;
};

#endif
