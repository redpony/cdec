#include "intersector.h"

#include "data_array.h"
#include "matching_comparator.h"
#include "phrase.h"
#include "phrase_location.h"
#include "precomputation.h"
#include "suffix_array.h"
#include "veb.h"
#include "vocabulary.h"

Intersector::Intersector(shared_ptr<Vocabulary> vocabulary,
                         shared_ptr<Precomputation> precomputation,
                         shared_ptr<SuffixArray> suffix_array,
                         shared_ptr<MatchingComparator> comparator,
                         bool use_baeza_yates) :
    vocabulary(vocabulary),
    suffix_array(suffix_array),
    use_baeza_yates(use_baeza_yates) {
  shared_ptr<DataArray> data_array = suffix_array->GetData();
  linear_merger = make_shared<LinearMerger>(vocabulary, data_array, comparator);
  binary_search_merger = make_shared<BinarySearchMerger>(
      vocabulary, linear_merger, data_array, comparator);
  ConvertIndexes(precomputation, data_array);
}

Intersector::Intersector(shared_ptr<Vocabulary> vocabulary,
                         shared_ptr<Precomputation> precomputation,
                         shared_ptr<SuffixArray> suffix_array,
                         shared_ptr<LinearMerger> linear_merger,
                         shared_ptr<BinarySearchMerger> binary_search_merger,
                         bool use_baeza_yates) :
    vocabulary(vocabulary),
    suffix_array(suffix_array),
    linear_merger(linear_merger),
    binary_search_merger(binary_search_merger),
    use_baeza_yates(use_baeza_yates) {
  ConvertIndexes(precomputation, suffix_array->GetData());
}

void Intersector::ConvertIndexes(shared_ptr<Precomputation> precomputation,
                                 shared_ptr<DataArray> data_array) {
  const Index& precomputed_index = precomputation->GetInvertedIndex();
  for (pair<vector<int>, vector<int> > entry: precomputed_index) {
    vector<int> phrase = ConvertPhrase(entry.first, data_array);
    inverted_index[phrase] = entry.second;
  }

  const Index& precomputed_collocations = precomputation->GetCollocations();
  for (pair<vector<int>, vector<int> > entry: precomputed_collocations) {
    vector<int> phrase = ConvertPhrase(entry.first, data_array);
    collocations[phrase] = entry.second;
  }
}

vector<int> Intersector::ConvertPhrase(const vector<int>& old_phrase,
                                       shared_ptr<DataArray> data_array) {
  vector<int> new_phrase;
  new_phrase.reserve(old_phrase.size());

  int arity = 0;
  for (int word_id: old_phrase) {
    if (word_id == Precomputation::NON_TERMINAL) {
      ++arity;
      new_phrase.push_back(vocabulary->GetNonterminalIndex(arity));
    } else {
      new_phrase.push_back(
          vocabulary->GetTerminalIndex(data_array->GetWord(word_id)));
    }
  }

  return new_phrase;
}

PhraseLocation Intersector::Intersect(
    const Phrase& prefix, PhraseLocation& prefix_location,
    const Phrase& suffix, PhraseLocation& suffix_location,
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

  vector<int> locations;
  ExtendPhraseLocation(prefix, prefix_location);
  ExtendPhraseLocation(suffix, suffix_location);
  shared_ptr<vector<int> > prefix_matchings = prefix_location.matchings;
  shared_ptr<vector<int> > suffix_matchings = suffix_location.matchings;
  int prefix_subpatterns = prefix_location.num_subpatterns;
  int suffix_subpatterns = prefix_location.num_subpatterns;
  if (use_baeza_yates) {
    binary_search_merger->Merge(locations, phrase, suffix,
        prefix_matchings->begin(), prefix_matchings->end(),
        suffix_matchings->begin(), suffix_matchings->end(),
        prefix_subpatterns, suffix_subpatterns);
  } else {
    linear_merger->Merge(locations, phrase, suffix, prefix_matchings->begin(),
        prefix_matchings->end(), suffix_matchings->begin(),
        suffix_matchings->end(), prefix_subpatterns, suffix_subpatterns);
  }
  return PhraseLocation(locations, phrase.Arity() + 1);
}

void Intersector::ExtendPhraseLocation(
    const Phrase& phrase, PhraseLocation& phrase_location) {
  int low = phrase_location.sa_low, high = phrase_location.sa_high;
  if (phrase_location.matchings != NULL) {
    return;
  }

  phrase_location.num_subpatterns = 1;
  phrase_location.sa_low = phrase_location.sa_high = 0;

  vector<int> symbols = phrase.Get();
  if (inverted_index.count(symbols)) {
    phrase_location.matchings =
        make_shared<vector<int> >(inverted_index[symbols]);
    return;
  }

  vector<int> matchings;
  matchings.reserve(high - low + 1);
  shared_ptr<VEB> veb = VEB::Create(suffix_array->GetSize());
  for (int i = low; i < high; ++i) {
    veb->Insert(suffix_array->GetSuffix(i));
  }

  int value = veb->GetMinimum();
  while (value != -1) {
    matchings.push_back(value);
    value = veb->GetSuccessor(value);
  }

  phrase_location.matchings = make_shared<vector<int> >(matchings);
}
