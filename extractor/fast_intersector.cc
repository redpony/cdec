#include "fast_intersector.h"

#include <cassert>

#include "data_array.h"
#include "phrase.h"
#include "phrase_location.h"
#include "precomputation.h"
#include "suffix_array.h"
#include "vocabulary.h"

namespace extractor {

FastIntersector::FastIntersector(shared_ptr<SuffixArray> suffix_array,
                                 shared_ptr<Precomputation> precomputation,
                                 shared_ptr<Vocabulary> vocabulary,
                                 int max_rule_span,
                                 int min_gap_size) :
    suffix_array(suffix_array),
    vocabulary(vocabulary),
    max_rule_span(max_rule_span),
    min_gap_size(min_gap_size) {
  Index precomputed_collocations = precomputation->GetCollocations();
  for (pair<vector<int>, vector<int> > entry: precomputed_collocations) {
    vector<int> phrase = ConvertPhrase(entry.first);
    collocations[phrase] = entry.second;
  }
}

FastIntersector::FastIntersector() {}

FastIntersector::~FastIntersector() {}

vector<int> FastIntersector::ConvertPhrase(const vector<int>& old_phrase) {
  vector<int> new_phrase;
  new_phrase.reserve(old_phrase.size());
  shared_ptr<DataArray> data_array = suffix_array->GetData();
  for (int word_id: old_phrase) {
    if (word_id < 0) {
      new_phrase.push_back(word_id);
    } else {
      new_phrase.push_back(
          vocabulary->GetTerminalIndex(data_array->GetWord(word_id)));
    }
  }
  return new_phrase;
}

PhraseLocation FastIntersector::Intersect(
    PhraseLocation& prefix_location,
    PhraseLocation& suffix_location,
    const Phrase& phrase) {
  vector<int> symbols = phrase.Get();

  // We should never attempt to do an intersect query for a pattern starting or
  // ending with a non terminal. The RuleFactory should handle these cases,
  // initializing the matchings list with the one for the pattern without the
  // starting or ending terminal.
  assert(vocabulary->IsTerminal(symbols.front())
      && vocabulary->IsTerminal(symbols.back()));

  if (collocations.count(symbols)) {
    return PhraseLocation(collocations[symbols], phrase.Arity() + 1);
  }

  bool prefix_ends_with_x =
      !vocabulary->IsTerminal(symbols[symbols.size() - 2]);
  bool suffix_starts_with_x = !vocabulary->IsTerminal(symbols[1]);
  if (EstimateNumOperations(prefix_location, prefix_ends_with_x) <=
      EstimateNumOperations(suffix_location, suffix_starts_with_x)) {
    return ExtendPrefixPhraseLocation(prefix_location, phrase,
                                      prefix_ends_with_x, symbols.back());
  } else {
    return ExtendSuffixPhraseLocation(suffix_location, phrase,
                                      suffix_starts_with_x, symbols.front());
  }
}

int FastIntersector::EstimateNumOperations(
    const PhraseLocation& phrase_location, bool has_margin_x) const {
  int num_locations = phrase_location.GetSize();
  return has_margin_x ? num_locations * max_rule_span : num_locations;
}

PhraseLocation FastIntersector::ExtendPrefixPhraseLocation(
    PhraseLocation& prefix_location, const Phrase& phrase,
    bool prefix_ends_with_x, int next_symbol) const {
  ExtendPhraseLocation(prefix_location);
  vector<int> positions = *prefix_location.matchings;
  int num_subpatterns = prefix_location.num_subpatterns;

  vector<int> new_positions;
  shared_ptr<DataArray> data_array = suffix_array->GetData();
  int data_array_symbol = data_array->GetWordId(
      vocabulary->GetTerminalValue(next_symbol));
  if (data_array_symbol == -1) {
    return PhraseLocation(new_positions, num_subpatterns);
  }

  pair<int, int> range = GetSearchRange(prefix_ends_with_x);
  for (size_t i = 0; i < positions.size(); i += num_subpatterns) {
    int sent_id = data_array->GetSentenceId(positions[i]);
    int sent_end = data_array->GetSentenceStart(sent_id + 1) - 1;
    int pattern_end = positions[i + num_subpatterns - 1] + range.first;
    if (prefix_ends_with_x) {
      pattern_end += phrase.GetChunkLen(phrase.Arity() - 1) - 1;
    } else {
      pattern_end += phrase.GetChunkLen(phrase.Arity()) - 2;
    }
    // Searches for the last symbol in the phrase after each prefix occurrence.
    for (int j = range.first; j < range.second; ++j) {
      if (pattern_end >= sent_end ||
          pattern_end - positions[i] >= max_rule_span) {
        break;
      }

      if (data_array->AtIndex(pattern_end) == data_array_symbol) {
        new_positions.insert(new_positions.end(), positions.begin() + i,
                             positions.begin() + i + num_subpatterns);
        if (prefix_ends_with_x) {
          new_positions.push_back(pattern_end);
        }
      }
      ++pattern_end;
    }
  }

  return PhraseLocation(new_positions, phrase.Arity() + 1);
}

PhraseLocation FastIntersector::ExtendSuffixPhraseLocation(
    PhraseLocation& suffix_location, const Phrase& phrase,
    bool suffix_starts_with_x, int prev_symbol) const {
  ExtendPhraseLocation(suffix_location);
  vector<int> positions = *suffix_location.matchings;
  int num_subpatterns = suffix_location.num_subpatterns;

  vector<int> new_positions;
  shared_ptr<DataArray> data_array = suffix_array->GetData();
  int data_array_symbol = data_array->GetWordId(
      vocabulary->GetTerminalValue(prev_symbol));
  if (data_array_symbol == -1) {
    return PhraseLocation(new_positions, num_subpatterns);
  }

  pair<int, int> range = GetSearchRange(suffix_starts_with_x);
  for (size_t i = 0; i < positions.size(); i += num_subpatterns) {
    int sent_id = data_array->GetSentenceId(positions[i]);
    int sent_start = data_array->GetSentenceStart(sent_id);
    int pattern_start = positions[i] - range.first;
    int pattern_end = positions[i + num_subpatterns - 1] +
        phrase.GetChunkLen(phrase.Arity()) - 1;
    // Searches for the first symbol in the phrase before each suffix
    // occurrence.
    for (int j = range.first; j < range.second; ++j) {
      if (pattern_start < sent_start ||
          pattern_end - pattern_start >= max_rule_span) {
        break;
      }

      if (data_array->AtIndex(pattern_start) == data_array_symbol) {
        new_positions.push_back(pattern_start);
        new_positions.insert(new_positions.end(),
                             positions.begin() + i + !suffix_starts_with_x,
                             positions.begin() + i + num_subpatterns);
      }
      --pattern_start;
    }
  }

  return PhraseLocation(new_positions, phrase.Arity() + 1);
}

void FastIntersector::ExtendPhraseLocation(PhraseLocation& location) const {
  if (location.matchings != NULL) {
    return;
  }

  location.num_subpatterns = 1;
  location.matchings = make_shared<vector<int> >();
  for (int i = location.sa_low; i < location.sa_high; ++i) {
    location.matchings->push_back(suffix_array->GetSuffix(i));
  }
  location.sa_low = location.sa_high = 0;
}

pair<int, int> FastIntersector::GetSearchRange(bool has_marginal_x) const {
  if (has_marginal_x) {
    return make_pair(min_gap_size + 1, max_rule_span);
  } else {
    return make_pair(1, 2);
  }
}

} // namespace extractor
