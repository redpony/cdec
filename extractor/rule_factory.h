#ifndef _RULE_FACTORY_H_
#define _RULE_FACTORY_H_

#include <memory>
#include <vector>
#include <unordered_set>

#include "matchings_trie.h"

using namespace std;

namespace extractor {

class Alignment;
class DataArray;
class FastIntersector;
class Grammar;
class MatchingsFinder;
class PhraseBuilder;
class Precomputation;
class Rule;
class RuleExtractor;
class Sampler;
class Scorer;
class State;
class SuffixArray;
class Vocabulary;

/**
 * Component containing most of the logic for extracting SCFG rules for a given
 * sentence.
 *
 * Given a sentence (as a vector of word ids), this class constructs all the
 * possible source phrases starting from this sentence. For each source phrase,
 * it finds all its occurrences in the source data and samples some of these
 * occurrences to extract aligned source-target phrase pairs. A trie cache is
 * used to avoid unnecessary computations if a source phrase can be constructed
 * more than once (e.g. some words occur more than once in the sentence).
 */
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
      bool require_tight_phrases);

  // For testing only.
  HieroCachingRuleFactory(
      shared_ptr<MatchingsFinder> finder,
      shared_ptr<FastIntersector> fast_intersector,
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

  // Constructs SCFG rules for a given sentence.
  // (See class description for more details.)
  virtual Grammar GetGrammar(
      const vector<int>& word_ids,
      const unordered_set<int>& blacklisted_sentence_ids);

 protected:
  HieroCachingRuleFactory();

 private:
  // Checks if the phrase (if previously encountered) or its prefix have any
  // occurrences in the source data.
  bool CannotHaveMatchings(shared_ptr<TrieNode> node, int word_id);

  // Checks if the phrase has previously been analyzed.
  bool RequiresLookup(shared_ptr<TrieNode> node, int word_id);

  // Creates a new state in the trie that corresponds to adding a trailing
  // nonterminal to the current phrase.
  void AddTrailingNonterminal(vector<int> symbols,
                              const Phrase& prefix,
                              const shared_ptr<TrieNode>& prefix_node,
                              bool starts_with_x);

  // Extends the current state by possibly adding a nonterminal followed by a
  // terminal.
  vector<State> ExtendState(const vector<int>& word_ids,
                            const State& state,
                            vector<int> symbols,
                            const Phrase& phrase,
                            const shared_ptr<TrieNode>& node);

  shared_ptr<MatchingsFinder> matchings_finder;
  shared_ptr<FastIntersector> fast_intersector;
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

} // namespace extractor

#endif
