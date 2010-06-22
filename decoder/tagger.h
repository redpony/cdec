#ifndef _TAGGER_H_
#define _TAGGER_H_

#include "translator.h"

struct TaggerImpl;
struct Tagger : public Translator {
  Tagger(const boost::program_options::variables_map& conf);
  bool TranslateImpl(const std::string& input,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* forest);
 private:
  boost::shared_ptr<TaggerImpl> pimpl_;
};

#endif
