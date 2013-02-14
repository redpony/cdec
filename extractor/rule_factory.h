#ifndef _RULE_FACTORY_H_
#define _RULE_FACTORY_H_

#include <memory>
#include <vector>

#include "matchings_trie.h"
#include "phrase_builder.h"

using namespace std;

class Alignment;
class DataArray;
class Grammar;
class MatchingsFinder;
class Intersector;
class Precomputation;
class Rule;
class RuleExtractor;
class Sampler;
class Scorer;
class State;
class SuffixArray;
class Vocabulary;

class HieroCachingRuleFactory {
 public:
  HieroCachingRuleFactory(
      shared_ptr<SuffixArray> source_suffix_array,
      shared_ptr<DataArray> target_data_array,
      shared_ptr<Alignment> alignment,
      const shared_ptr<Vocabulary>& vocabulary,
      shared_ptr<Precomputation> precomputation,
      shared_ptr<Scorer> scorer,
      int min_gap_size,
      int max_rule_span,
      int max_nonterminals,
      int max_rule_symbols,
      int max_samples,
      bool use_beaza_yates,
      bool require_tight_phrases);

  // For testing only.
  HieroCachingRuleFactory(
      shared_ptr<MatchingsFinder> finder,
      shared_ptr<Intersector> intersector,
      shared_ptr<PhraseBuilder> phrase_builder,
      shared_ptr<RuleExtractor> rule_extractor,
      shared_ptr<Vocabulary> vocabulary,
      shared_ptr<Sampler> sampler,
      shared_ptr<Scorer> scorer,
      int min_gap_size,
      int max_rule_span,
      int max_nonterminals,
      int max_chunks,
      int max_rule_symbols);

  virtual ~HieroCachingRuleFactory();

  virtual Grammar GetGrammar(const vector<int>& word_ids);

 protected:
  HieroCachingRuleFactory();

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

  shared_ptr<MatchingsFinder> matchings_finder;
  shared_ptr<Intersector> intersector;
  MatchingsTrie trie;
  shared_ptr<PhraseBuilder> phrase_builder;
  shared_ptr<RuleExtractor> rule_extractor;
  shared_ptr<Vocabulary> vocabulary;
  shared_ptr<Sampler> sampler;
  shared_ptr<Scorer> scorer;
  int min_gap_size;
  int max_rule_span;
  int max_nonterminals;
  int max_chunks;
  int max_rule_symbols;
};

#endif
