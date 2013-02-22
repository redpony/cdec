#ifndef _GRAMMAR_EXTRACTOR_H_
#define _GRAMMAR_EXTRACTOR_H_

#include <string>
#include <vector>

#include "rule_factory.h"

using namespace std;

class Alignment;
class DataArray;
class Grammar;
class Precomputation;
class Rule;
class SuffixArray;
class Vocabulary;

class GrammarExtractor {
 public:
  GrammarExtractor(
      shared_ptr<SuffixArray> source_suffix_array,
      shared_ptr<DataArray> target_data_array,
      shared_ptr<Alignment> alignment,
      shared_ptr<Precomputation> precomputation,
      shared_ptr<Scorer> scorer,
      int min_gap_size,
      int max_rule_span,
      int max_nonterminals,
      int max_rule_symbols,
      int max_samples,
      bool require_tight_phrases);

  // For testing only.
  GrammarExtractor(shared_ptr<Vocabulary> vocabulary,
                   shared_ptr<HieroCachingRuleFactory> rule_factory);

  Grammar GetGrammar(const string& sentence);

 private:
  vector<string> TokenizeSentence(const string& sentence);

  vector<int> AnnotateWords(const vector<string>& words);

  shared_ptr<Vocabulary> vocabulary;
  shared_ptr<HieroCachingRuleFactory> rule_factory;
};

#endif
