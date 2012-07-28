#ifndef _TRANSLATOR_H_
#define _TRANSLATOR_H_

#include <string>
#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/program_options/variables_map.hpp>

#include "grammar.h"

class Hypergraph;
class SentenceMetadata;

// Workflow: for each sentence to be translated
//   1) call ProcessMarkupHints(markup)
//   2) call Translate(...)
//   3) call SentenceComplete()
class Translator {
 public:
  Translator() : state_(kUninitialized) {}
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
  bool Translate(const std::string& src,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* minus_lm_forest);

  // This is called before Translate(...) with the sentence-
  // level markup passed in. This can be used to set sentence-
  // specific behavior of the translator.
  void ProcessMarkupHints(const std::map<std::string, std::string>& kv);

  // Free any sentence-specific resources
  void SentenceComplete();
  virtual std::string GetDecoderType() const;
 protected:
  virtual bool TranslateImpl(const std::string& src,
                             SentenceMetadata* smeta,
                             const std::vector<double>& weights,
                             Hypergraph* minus_lm_forest) = 0;
  virtual void ProcessMarkupHintsImpl(const std::map<std::string, std::string>& kv);
  virtual void SentenceCompleteImpl();
 private:
  enum State { kUninitialized, kReadyToTranslate, kTranslated };
  State state_;
};

class SCFGTranslatorImpl;
class SCFGTranslator : public Translator {
 public:
  SCFGTranslator(const boost::program_options::variables_map& conf);
  void AddSupplementalGrammar(GrammarPtr gp);
  void AddSupplementalGrammarFromString(const std::string& grammar);
  virtual std::string GetDecoderType() const;
 protected:
  bool TranslateImpl(const std::string& src,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* minus_lm_forest);
  void ProcessMarkupHintsImpl(const std::map<std::string, std::string>& kv);
  void SentenceCompleteImpl();
 private:
  boost::shared_ptr<SCFGTranslatorImpl> pimpl_;
};

class FSTTranslatorImpl;
class FSTTranslator : public Translator {
 public:
  FSTTranslator(const boost::program_options::variables_map& conf);
 private:
  bool TranslateImpl(const std::string& src,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* minus_lm_forest);
 private:
  boost::shared_ptr<FSTTranslatorImpl> pimpl_;
};

class RescoreTranslatorImpl;
class RescoreTranslator : public Translator {
 public:
  RescoreTranslator(const boost::program_options::variables_map& conf);
 private:
  bool TranslateImpl(const std::string& src,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* minus_lm_forest);
 private:
  boost::shared_ptr<RescoreTranslatorImpl> pimpl_;
};

#endif
