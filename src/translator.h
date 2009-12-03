#ifndef _TRANSLATOR_H_
#define _TRANSLATOR_H_

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/program_options/variables_map.hpp>

class Hypergraph;
class SentenceMetadata;

class Translator {
 public:
  virtual ~Translator();
  // returns true if goal reached, false otherwise
  // minus_lm_forest will contain the unpruned forest. the
  // feature values from the phrase table / grammar / etc
  // should be in the forest already - the "late" features
  // should not just copy values that are available without
  // any context or computation.
  // SentenceMetadata contains information about the sentence,
  // but it is an input/output parameter since the Translator
  // is also responsible for setting the value of src_len.
  virtual bool Translate(const std::string& src,
                         SentenceMetadata* smeta,
                         const std::vector<double>& weights,
                         Hypergraph* minus_lm_forest) = 0;
};

class SCFGTranslatorImpl;
class SCFGTranslator : public Translator {
 public:
  SCFGTranslator(const boost::program_options::variables_map& conf);
  bool Translate(const std::string& src,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* minus_lm_forest);
 private:
  boost::shared_ptr<SCFGTranslatorImpl> pimpl_;
};

class FSTTranslatorImpl;
class FSTTranslator : public Translator {
 public:
  FSTTranslator(const boost::program_options::variables_map& conf);
  bool Translate(const std::string& src,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* minus_lm_forest);
 private:
  boost::shared_ptr<FSTTranslatorImpl> pimpl_;
};

#endif
