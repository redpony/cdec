#include "target_phrase_extractor.h"

#include <unordered_set>

#include "alignment.h"
#include "data_array.h"
#include "phrase.h"
#include "phrase_builder.h"
#include "rule_extractor_helper.h"
#include "vocabulary.h"

using namespace std;

namespace extractor {

TargetPhraseExtractor::TargetPhraseExtractor(
    shared_ptr<DataArray> target_data_array,
    shared_ptr<Alignment> alignment,
    shared_ptr<PhraseBuilder> phrase_builder,
    shared_ptr<RuleExtractorHelper> helper,
    shared_ptr<Vocabulary> vocabulary,
    int max_rule_span,
    bool require_tight_phrases) :
    target_data_array(target_data_array),
    alignment(alignment),
    phrase_builder(phrase_builder),
    helper(helper),
    vocabulary(vocabulary),
    max_rule_span(max_rule_span),
    require_tight_phrases(require_tight_phrases) {}

TargetPhraseExtractor::TargetPhraseExtractor() {}

TargetPhraseExtractor::~TargetPhraseExtractor() {}

vector<pair<Phrase, PhraseAlignment>> TargetPhraseExtractor::ExtractPhrases(
    const vector<pair<int, int>>& target_gaps, const vector<int>& target_low,
    int target_phrase_low, int target_phrase_high,
    const unordered_map<int, int>& source_indexes, int sentence_id) const {
  int target_sent_len = target_data_array->GetSentenceLength(sentence_id);

  vector<int> target_gap_order = helper->GetGapOrder(target_gaps);

  int target_x_low = target_phrase_low, target_x_high = target_phrase_high;
  if (!require_tight_phrases) {
    // Extend loose target phrase to the left.
    while (target_x_low > 0 &&
           target_phrase_high - target_x_low < max_rule_span &&
           target_low[target_x_low - 1] == -1) {
      --target_x_low;
    }
    // Extend loose target phrase to the right.
    while (target_x_high < target_sent_len &&
           target_x_high - target_phrase_low < max_rule_span &&
           target_low[target_x_high] == -1) {
      ++target_x_high;
    }
  }

  vector<pair<int, int>> gaps(target_gaps.size());
  for (size_t i = 0; i < gaps.size(); ++i) {
    gaps[i] = target_gaps[target_gap_order[i]];
    if (!require_tight_phrases) {
      // Extend gap to the left.
      while (gaps[i].first > target_x_low &&
             target_low[gaps[i].first - 1] == -1) {
        --gaps[i].first;
      }
      // Extend gap to the right.
      while (gaps[i].second < target_x_high &&
             target_low[gaps[i].second] == -1) {
        ++gaps[i].second;
      }
    }
  }

  // Compute the range in which each chunk may start or end. (Even indexes
  // represent the range in which the chunk may start, odd indexes represent the
  // range in which the chunk may end.)
  vector<pair<int, int>> ranges(2 * gaps.size() + 2);
  ranges.front() = make_pair(target_x_low, target_phrase_low);
  ranges.back() = make_pair(target_phrase_high, target_x_high);
  for (size_t i = 0; i < gaps.size(); ++i) {
    int j = target_gap_order[i];
    ranges[i * 2 + 1] = make_pair(gaps[i].first, target_gaps[j].first);
    ranges[i * 2 + 2] = make_pair(target_gaps[j].second, gaps[i].second);
  }

  vector<pair<Phrase, PhraseAlignment>> target_phrases;
  vector<int> subpatterns(ranges.size());
  GeneratePhrases(target_phrases, ranges, 0, subpatterns, target_gap_order,
                  target_phrase_low, target_phrase_high, source_indexes,
                  sentence_id);
  return target_phrases;
}

void TargetPhraseExtractor::GeneratePhrases(
    vector<pair<Phrase, PhraseAlignment>>& target_phrases,
    const vector<pair<int, int>>& ranges, int index, vector<int>& subpatterns,
    const vector<int>& target_gap_order, int target_phrase_low,
    int target_phrase_high, const unordered_map<int, int>& source_indexes,
    int sentence_id) const {
  if (index >= ranges.size()) {
    if (subpatterns.back() - subpatterns.front() > max_rule_span) {
      return;
    }

    vector<int> symbols;
    unordered_map<int, int> target_indexes;

    // Construct target phrase chunk by chunk.
    int target_sent_start = target_data_array->GetSentenceStart(sentence_id);
    for (size_t i = 0; i * 2 < subpatterns.size(); ++i) {
      for (size_t j = subpatterns[i * 2]; j < subpatterns[i * 2 + 1]; ++j) {
        target_indexes[j] = symbols.size();
        string target_word = target_data_array->GetWordAtIndex(
            target_sent_start + j);
        symbols.push_back(vocabulary->GetTerminalIndex(target_word));
      }
      if (i < target_gap_order.size()) {
        symbols.push_back(vocabulary->GetNonterminalIndex(
            target_gap_order[i] + 1));
      }
    }

    // Construct the alignment between the source and the target phrase.
    vector<pair<int, int>> links = alignment->GetLinks(sentence_id);
    vector<pair<int, int>> alignment;
    for (pair<int, int> link: links) {
      if (target_indexes.count(link.second)) {
        alignment.push_back(make_pair(source_indexes.find(link.first)->second,
                                      target_indexes[link.second]));
      }
    }

    Phrase target_phrase = phrase_builder->Build(symbols);
    target_phrases.push_back(make_pair(target_phrase, alignment));
    return;
  }

  subpatterns[index] = ranges[index].first;
  if (index > 0) {
    subpatterns[index] = max(subpatterns[index], subpatterns[index - 1]);
  }
  // Choose every possible combination of [start, end) for the current chunk.
  while (subpatterns[index] <= ranges[index].second) {
    subpatterns[index + 1] = max(subpatterns[index], ranges[index + 1].first);
    while (subpatterns[index + 1] <= ranges[index + 1].second) {
      GeneratePhrases(target_phrases, ranges, index + 2, subpatterns,
                      target_gap_order, target_phrase_low, target_phrase_high,
                      source_indexes, sentence_id);
      ++subpatterns[index + 1];
    }
    ++subpatterns[index];
  }
}

} // namespace extractor
