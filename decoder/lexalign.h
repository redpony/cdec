#ifndef _LEXALIGN_H_
#define _LEXALIGN_H_

#include "translator.h"
#include "lattice.h"

struct LexicalAlignImpl;
struct LexicalAlign : public Translator {
  LexicalAlign(const boost::program_options::variables_map& conf);
  bool TranslateImpl(const std::string& input,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* forest);
 private:
  boost::shared_ptr<LexicalAlignImpl> pimpl_;
};

#endif
