#include "rule_factory.h"

#include <chrono>
#include <memory>
#include <queue>
#include <vector>

#include "grammar.h"
#include "fast_intersector.h"
#include "matchings_finder.h"
#include "phrase.h"
#include "phrase_builder.h"
#include "rule.h"
#include "rule_extractor.h"
#include "phrase_location_sampler.h"
#include "sampler.h"
#include "scorer.h"
#include "suffix_array.h"
#include "time_util.h"
#include "vocabulary.h"
#include "data_array.h"

using namespace std;
using namespace chrono;

namespace extractor {

typedef high_resolution_clock Clock;

struct State {
  State(int start, int end, const vector<int>& phrase,
      const vector<int>& subpatterns_start, shared_ptr<TrieNode> node,
      bool starts_with_x) :
      start(start), end(end), phrase(phrase),
      subpatterns_start(subpatterns_start), node(node),
      starts_with_x(starts_with_x) {}

  int start, end;
  vector<int> phrase, subpatterns_start;
  shared_ptr<TrieNode> node;
  bool starts_with_x;
};

HieroCachingRuleFactory::HieroCachingRuleFactory(
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
    bool require_tight_phrases) :
    vocabulary(vocabulary),
    scorer(scorer),
    min_gap_size(min_gap_size),
    max_rule_span(max_rule_span),
    max_nonterminals(max_nonterminals),
    max_chunks(max_nonterminals + 1),
    max_rule_symbols(max_rule_symbols) {
  matchings_finder = make_shared<MatchingsFinder>(source_suffix_array);
  fast_intersector = make_shared<FastIntersector>(source_suffix_array,
      precomputation, vocabulary, max_rule_span, min_gap_size);
  phrase_builder = make_shared<PhraseBuilder>(vocabulary);
  rule_extractor = make_shared<RuleExtractor>(source_suffix_array->GetData(),
      target_data_array, alignment, phrase_builder, scorer, vocabulary,
      max_rule_span, min_gap_size, max_nonterminals, max_rule_symbols, true,
      false, require_tight_phrases);
  sampler = make_shared<PhraseLocationSampler>(
      source_suffix_array, max_samples);
}

HieroCachingRuleFactory::HieroCachingRuleFactory(
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
    int max_rule_symbols) :
    matchings_finder(finder),
    fast_intersector(fast_intersector),
    phrase_builder(phrase_builder),
    rule_extractor(rule_extractor),
    vocabulary(vocabulary),
    sampler(sampler),
    scorer(scorer),
    min_gap_size(min_gap_size),
    max_rule_span(max_rule_span),
    max_nonterminals(max_nonterminals),
    max_chunks(max_chunks),
    max_rule_symbols(max_rule_symbols) {}

HieroCachingRuleFactory::HieroCachingRuleFactory() {}

HieroCachingRuleFactory::~HieroCachingRuleFactory() {}

Grammar HieroCachingRuleFactory::GetGrammar(
    const vector<int>& word_ids,
    const unordered_set<int>& blacklisted_sentence_ids) {
  Clock::time_point start_time = Clock::now();
  double total_extract_time = 0;
  double total_intersect_time = 0;
  double total_lookup_time = 0;

  MatchingsTrie trie;
  shared_ptr<TrieNode> root = trie.GetRoot();

  int first_x = vocabulary->GetNonterminalIndex(1);
  shared_ptr<TrieNode> x_root(new TrieNode(root));
  root->AddChild(first_x, x_root);

  queue<State> states;
  for (size_t i = 0; i < word_ids.size(); ++i) {
    states.push(State(i, i, vector<int>(), vector<int>(1, i), root, false));
  }
  for (size_t i = min_gap_size; i < word_ids.size(); ++i) {
    states.push(State(i - min_gap_size, i, vector<int>(1, first_x),
        vector<int>(1, i), x_root, true));
  }

  vector<Rule> rules;
  while (!states.empty()) {
    State state = states.front();
    states.pop();

    shared_ptr<TrieNode> node = state.node;
    vector<int> phrase = state.phrase;
    int word_id = word_ids[state.end];
    phrase.push_back(word_id);
    Phrase next_phrase = phrase_builder->Build(phrase);
    shared_ptr<TrieNode> next_node;

    if (CannotHaveMatchings(node, word_id)) {
      if (!node->HasChild(word_id)) {
        node->AddChild(word_id, shared_ptr<TrieNode>());
      }
      continue;
    }

    if (RequiresLookup(node, word_id)) {
      shared_ptr<TrieNode> next_suffix_link = node->suffix_link == NULL ?
          trie.GetRoot() : node->suffix_link->GetChild(word_id);
      if (state.starts_with_x) {
        // If the phrase starts with a non terminal, we simply use the matchings
        // from the suffix link.
        next_node = make_shared<TrieNode>(
            next_suffix_link, next_phrase, next_suffix_link->matchings);
      } else {
        PhraseLocation phrase_location;
        if (next_phrase.Arity() > 0) {
          // For phrases containing a nonterminal, we use either the occurrences
          // of the prefix or the suffix to determine the occurrences of the
          // phrase.
          Clock::time_point intersect_start = Clock::now();
          phrase_location = fast_intersector->Intersect(
              node->matchings, next_suffix_link->matchings, next_phrase);
          Clock::time_point intersect_stop = Clock::now();
          total_intersect_time += GetDuration(intersect_start, intersect_stop);
        } else {
          // For phrases not containing any nonterminals, we simply query the
          // suffix array using the suffix array range of the prefix as a
          // starting point.
          Clock::time_point lookup_start = Clock::now();
          phrase_location = matchings_finder->Find(
              node->matchings,
              vocabulary->GetTerminalValue(word_id),
              state.phrase.size());
          Clock::time_point lookup_stop = Clock::now();
          total_lookup_time += GetDuration(lookup_start, lookup_stop);
        }

        if (phrase_location.IsEmpty()) {
          continue;
        }

        // Create new trie node to store data about the current phrase.
        next_node = make_shared<TrieNode>(
            next_suffix_link, next_phrase, phrase_location);
      }
      // Add the new trie node to the trie cache.
      node->AddChild(word_id, next_node);

      // Automatically adds a trailing non terminal if allowed. Simply copy the
      // matchings from the prefix node.
      AddTrailingNonterminal(phrase, next_phrase, next_node,
                             state.starts_with_x);

      Clock::time_point extract_start = Clock::now();
      if (!state.starts_with_x) {
        // Extract rules for the sampled set of occurrences.
        PhraseLocation sample = sampler->Sample(
            next_node->matchings, blacklisted_sentence_ids);
        vector<Rule> new_rules =
            rule_extractor->ExtractRules(next_phrase, sample);
        rules.insert(rules.end(), new_rules.begin(), new_rules.end());
      }
      Clock::time_point extract_stop = Clock::now();
      total_extract_time += GetDuration(extract_start, extract_stop);
    } else {
      next_node = node->GetChild(word_id);
    }

    // Create more states (phrases) to be analyzed.
    vector<State> new_states = ExtendState(word_ids, state, phrase, next_phrase,
                                           next_node);
    for (State new_state: new_states) {
      states.push(new_state);
    }
  }

  Clock::time_point stop_time = Clock::now();
  #pragma omp critical (stderr_write)
  {
    cerr << "Total time for rule lookup, extraction, and scoring = "
         << GetDuration(start_time, stop_time) << " seconds" << endl;
    cerr << "Extract time = " << total_extract_time << " seconds" << endl;
    cerr << "Intersect time = " << total_intersect_time << " seconds" << endl;
    cerr << "Lookup time = " << total_lookup_time << " seconds" << endl;
  }
  return Grammar(rules, scorer->GetFeatureNames());
}

bool HieroCachingRuleFactory::CannotHaveMatchings(
    shared_ptr<TrieNode> node, int word_id) {
  if (node->HasChild(word_id) && node->GetChild(word_id) == NULL) {
    return true;
  }

  shared_ptr<TrieNode> suffix_link = node->suffix_link;
  return suffix_link != NULL && suffix_link->GetChild(word_id) == NULL;
}

bool HieroCachingRuleFactory::RequiresLookup(
    shared_ptr<TrieNode> node, int word_id) {
  return !node->HasChild(word_id);
}

void HieroCachingRuleFactory::AddTrailingNonterminal(
    vector<int> symbols,
    const Phrase& prefix,
    const shared_ptr<TrieNode>& prefix_node,
    bool starts_with_x) {
  if (prefix.Arity() >= max_nonterminals) {
    return;
  }

  int var_id = vocabulary->GetNonterminalIndex(prefix.Arity() + 1);
  symbols.push_back(var_id);
  Phrase var_phrase = phrase_builder->Build(symbols);

  int suffix_var_id = vocabulary->GetNonterminalIndex(
      prefix.Arity() + (starts_with_x == 0));
  shared_ptr<TrieNode> var_suffix_link =
      prefix_node->suffix_link->GetChild(suffix_var_id);

  prefix_node->AddChild(var_id, make_shared<TrieNode>(
      var_suffix_link, var_phrase, prefix_node->matchings));
}

vector<State> HieroCachingRuleFactory::ExtendState(
    const vector<int>& word_ids,
    const State& state,
    vector<int> symbols,
    const Phrase& phrase,
    const shared_ptr<TrieNode>& node) {
  int span = state.end - state.start;
  vector<State> new_states;
  if (symbols.size() >= max_rule_symbols || state.end + 1 >= word_ids.size() ||
      span >= max_rule_span) {
    return new_states;
  }

  // New state for adding the next symbol.
  new_states.push_back(State(state.start, state.end + 1, symbols,
      state.subpatterns_start, node, state.starts_with_x));

  int num_subpatterns = phrase.Arity() + (state.starts_with_x == 0);
  if (symbols.size() + 1 >= max_rule_symbols ||
      phrase.Arity() >= max_nonterminals ||
      num_subpatterns >= max_chunks) {
    return new_states;
  }

  // New states for adding a nonterminal followed by a new symbol.
  int var_id = vocabulary->GetNonterminalIndex(phrase.Arity() + 1);
  symbols.push_back(var_id);
  vector<int> subpatterns_start = state.subpatterns_start;
  size_t i = state.end + 1 + min_gap_size;
  while (i < word_ids.size() && i - state.start <= max_rule_span) {
    subpatterns_start.push_back(i);
    new_states.push_back(State(state.start, i, symbols, subpatterns_start,
        node->GetChild(var_id), state.starts_with_x));
    subpatterns_start.pop_back();
    ++i;
  }

  return new_states;
}

} // namespace extractor
