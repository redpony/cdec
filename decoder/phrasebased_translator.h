#ifndef _PHRASEBASED_TRANSLATOR_H_
#define _PHRASEBASED_TRANSLATOR_H_

#include "translator.h"

class PhraseBasedTranslatorImpl;
class PhraseBasedTranslator : public Translator {
 public:
  PhraseBasedTranslator(const boost::program_options::variables_map& conf);
  bool TranslateImpl(const std::string& input,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* minus_lm_forest);
 private:
  boost::shared_ptr<PhraseBasedTranslatorImpl> pimpl_;
};

#endif
