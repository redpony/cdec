#ifndef _GRAMMAR_EXTRACTOR_H_
#define _GRAMMAR_EXTRACTOR_H_

#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

using namespace std;

namespace extractor {

class Alignment;
class DataArray;
class Grammar;
class HieroCachingRuleFactory;
class Precomputation;
class Scorer;
class SuffixArray;
class Vocabulary;

/**
 * Class wrapping all the logic for extracting the synchronous context free
 * grammars.
 */
class GrammarExtractor {
 public:
  GrammarExtractor(
      shared_ptr<SuffixArray> source_suffix_array,
      shared_ptr<DataArray> target_data_array,
      shared_ptr<Alignment> alignment,
      shared_ptr<Precomputation> precomputation,
      shared_ptr<Scorer> scorer,
      shared_ptr<Vocabulary> vocabulary,
      int min_gap_size,
      int max_rule_span,
      int max_nonterminals,
      int max_rule_symbols,
      int max_samples,
      bool require_tight_phrases);

  // For testing only.
  GrammarExtractor(shared_ptr<Vocabulary> vocabulary,
                   shared_ptr<HieroCachingRuleFactory> rule_factory);

  // Converts the sentence to a vector of word ids and uses the RuleFactory to
  // extract the SCFG rules which may be used to decode the sentence.
  Grammar GetGrammar(
      const string& sentence,
      const unordered_set<int>& blacklisted_sentence_ids);

 private:
  // Splits the sentence in a vector of words.
  vector<string> TokenizeSentence(const string& sentence);

  // Maps the words to word ids.
  vector<int> AnnotateWords(const vector<string>& words);

  shared_ptr<Vocabulary> vocabulary;
  shared_ptr<HieroCachingRuleFactory> rule_factory;
};

} // namespace extractor

#endif
