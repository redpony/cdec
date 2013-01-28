#ifndef _RULE_FACTORY_H_
#define _RULE_FACTORY_H_

#include <memory>
#include <vector>

#include "matchings_finder.h"
#include "intersector.h"
#include "matchings_trie.h"
#include "phrase_builder.h"
#include "rule_extractor.h"

using namespace std;

class Alignment;
class DataArray;
class Precomputation;
class State;
class SuffixArray;
class Vocabulary;

class HieroCachingRuleFactory {
 public:
  HieroCachingRuleFactory(
      shared_ptr<SuffixArray> source_suffix_array,
      shared_ptr<DataArray> target_data_array,
      const Alignment& alignment,
      const shared_ptr<Vocabulary>& vocabulary,
      const Precomputation& precomputation,
      int min_gap_size,
      int max_rule_span,
      int max_nonterminals,
      int max_rule_symbols,
      bool use_beaza_yates);

  void GetGrammar(const vector<int>& word_ids);

 private:
  bool CannotHaveMatchings(shared_ptr<TrieNode> node, int word_id);

  bool RequiresLookup(shared_ptr<TrieNode> node, int word_id);

  void AddTrailingNonterminal(vector<int> symbols,
                              const Phrase& prefix,
                              const shared_ptr<TrieNode>& prefix_node,
                              bool starts_with_x);

  vector<State> ExtendState(const vector<int>& word_ids,
                            const State& state,
                            vector<int> symbols,
                            const Phrase& phrase,
                            const shared_ptr<TrieNode>& node);

  MatchingsFinder matchings_finder;
  Intersector intersector;
  MatchingsTrie trie;
  PhraseBuilder phrase_builder;
  RuleExtractor rule_extractor;
  shared_ptr<Vocabulary> vocabulary;
  int min_gap_size;
  int max_rule_span;
  int max_nonterminals;
  int max_chunks;
  int max_rule_symbols;
};

#endif
