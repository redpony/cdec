#ifndef _GRAMMAR_EXTRACTOR_H_
#define _GRAMMAR_EXTRACTOR_H_

#include <string>
#include <vector>

#include "rule_factory.h"
#include "vocabulary.h"

using namespace std;

class Alignment;
class DataArray;
class Precomputation;
class SuffixArray;

class GrammarExtractor {
 public:
  GrammarExtractor(
      shared_ptr<SuffixArray> source_suffix_array,
      shared_ptr<DataArray> target_data_array,
      const Alignment& alignment,
      const Precomputation& precomputation,
      int min_gap_size,
      int max_rule_span,
      int max_nonterminals,
      int max_rule_symbols,
      bool use_baeza_yates);

  void GetGrammar(const string& sentence);

 private:
  vector<int> AnnotateWords(const vector<string>& words);

  shared_ptr<Vocabulary> vocabulary;
  HieroCachingRuleFactory rule_factory;
};

#endif
