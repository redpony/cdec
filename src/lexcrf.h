#ifndef _LEXCRF_H_
#define _LEXCRF_H_

#include "translator.h"
#include "lattice.h"

struct LexicalCRFImpl;
struct LexicalCRF : public Translator {
  LexicalCRF(const boost::program_options::variables_map& conf);
  bool Translate(const std::string& input,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* forest);
 private:
  boost::shared_ptr<LexicalCRFImpl> pimpl_;
};

#endif
