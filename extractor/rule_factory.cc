#include "rule_factory.h"

#include <cassert>
#include <memory>
#include <queue>
#include <vector>

#include "matching_comparator.h"
#include "phrase.h"
#include "suffix_array.h"
#include "vocabulary.h"

using namespace std;
using namespace tr1;

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
    const Alignment& alignment,
    const shared_ptr<Vocabulary>& vocabulary,
    const Precomputation& precomputation,
    int min_gap_size,
    int max_rule_span,
    int max_nonterminals,
    int max_rule_symbols,
    bool use_baeza_yates) :
    matchings_finder(source_suffix_array),
    intersector(vocabulary, precomputation, source_suffix_array,
                make_shared<MatchingComparator>(min_gap_size, max_rule_span),
                use_baeza_yates),
    phrase_builder(vocabulary),
    rule_extractor(source_suffix_array, target_data_array, alignment),
    vocabulary(vocabulary),
    min_gap_size(min_gap_size),
    max_rule_span(max_rule_span),
    max_nonterminals(max_nonterminals),
    max_chunks(max_nonterminals + 1),
    max_rule_symbols(max_rule_symbols) {}

void HieroCachingRuleFactory::GetGrammar(const vector<int>& word_ids) {
  // Clear cache for every new sentence.
  trie.Reset();
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

  while (!states.empty()) {
    State state = states.front();
    states.pop();

    shared_ptr<TrieNode> node = state.node;
    vector<int> phrase = state.phrase;
    int word_id = word_ids[state.end];
    phrase.push_back(word_id);
    Phrase next_phrase = phrase_builder.Build(phrase);
    shared_ptr<TrieNode> next_node;

    if (CannotHaveMatchings(node, word_id)) {
      if (!node->HasChild(word_id)) {
        node->AddChild(word_id, shared_ptr<TrieNode>());
      }
      continue;
    }

    if (RequiresLookup(node, word_id)) {
      shared_ptr<TrieNode> next_suffix_link =
          node->suffix_link->GetChild(word_id);
      if (state.starts_with_x) {
        // If the phrase starts with a non terminal, we simply use the matchings
        // from the suffix link.
        next_node = shared_ptr<TrieNode>(new TrieNode(
            next_suffix_link, next_phrase, next_suffix_link->matchings));
      } else {
        PhraseLocation phrase_location;
        if (next_phrase.Arity() > 0) {
          phrase_location = intersector.Intersect(
              node->phrase,
              node->matchings,
              next_suffix_link->phrase,
              next_suffix_link->matchings,
              next_phrase);
        } else {
          phrase_location = matchings_finder.Find(
              node->matchings,
              vocabulary->GetTerminalValue(word_id),
              state.phrase.size());
        }

        if (phrase_location.IsEmpty()) {
          continue;
        }
        next_node = shared_ptr<TrieNode>(new TrieNode(
            next_suffix_link, next_phrase, phrase_location));
      }
      node->AddChild(word_id, next_node);

      // Automatically adds a trailing non terminal if allowed. Simply copy the
      // matchings from the prefix node.
      AddTrailingNonterminal(phrase, next_phrase, next_node,
                             state.starts_with_x);

      if (!state.starts_with_x) {
        rule_extractor.ExtractRules();
      }
    } else {
      next_node = node->GetChild(word_id);
    }

    vector<State> new_states = ExtendState(word_ids, state, phrase, next_phrase,
                                           next_node);
    for (State new_state: new_states) {
      states.push(new_state);
    }
  }
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
  Phrase var_phrase = phrase_builder.Build(symbols);

  int suffix_var_id = vocabulary->GetNonterminalIndex(
      prefix.Arity() + starts_with_x == 0);
  shared_ptr<TrieNode> var_suffix_link =
      prefix_node->suffix_link->GetChild(suffix_var_id);

  prefix_node->AddChild(var_id, shared_ptr<TrieNode>(new TrieNode(
      var_suffix_link, var_phrase, prefix_node->matchings)));
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

  new_states.push_back(State(state.start, state.end + 1, symbols,
      state.subpatterns_start, node, state.starts_with_x));

  int num_subpatterns = phrase.Arity() + state.starts_with_x == 0;
  if (symbols.size() + 1 >= max_rule_symbols ||
      phrase.Arity() >= max_nonterminals ||
      num_subpatterns >= max_chunks) {
    return new_states;
  }

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
