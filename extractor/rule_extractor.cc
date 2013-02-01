#include "rule_extractor.h"

#include <map>
#include <tr1/unordered_set>

#include "alignment.h"
#include "data_array.h"
#include "features/feature.h"
#include "phrase_builder.h"
#include "phrase_location.h"
#include "rule.h"
#include "scorer.h"
#include "vocabulary.h"

using namespace std;
using namespace tr1;

RuleExtractor::RuleExtractor(
    shared_ptr<DataArray> source_data_array,
    shared_ptr<DataArray> target_data_array,
    shared_ptr<Alignment> alignment,
    shared_ptr<PhraseBuilder> phrase_builder,
    shared_ptr<Scorer> scorer,
    shared_ptr<Vocabulary> vocabulary,
    int max_rule_span,
    int min_gap_size,
    int max_nonterminals,
    int max_rule_symbols,
    bool require_aligned_terminal,
    bool require_aligned_chunks,
    bool require_tight_phrases) :
    source_data_array(source_data_array),
    target_data_array(target_data_array),
    alignment(alignment),
    phrase_builder(phrase_builder),
    scorer(scorer),
    vocabulary(vocabulary),
    max_rule_span(max_rule_span),
    min_gap_size(min_gap_size),
    max_nonterminals(max_nonterminals),
    max_rule_symbols(max_rule_symbols),
    require_aligned_terminal(require_aligned_terminal),
    require_aligned_chunks(require_aligned_chunks),
    require_tight_phrases(require_tight_phrases) {}

vector<Rule> RuleExtractor::ExtractRules(const Phrase& phrase,
                                         const PhraseLocation& location) const {
  int num_subpatterns = location.num_subpatterns;
  vector<int> matchings = *location.matchings;

  map<Phrase, double> source_phrase_counter;
  map<Phrase, map<Phrase, map<PhraseAlignment, int> > > alignments_counter;
  for (auto i = matchings.begin(); i != matchings.end(); i += num_subpatterns) {
    vector<int> matching(i, i + num_subpatterns);
    vector<Extract> extracts = ExtractAlignments(phrase, matching);

    for (Extract e: extracts) {
      source_phrase_counter[e.source_phrase] += e.pairs_count;
      alignments_counter[e.source_phrase][e.target_phrase][e.alignment] += 1;
    }
  }

  vector<Rule> rules;
  for (auto source_phrase_entry: alignments_counter) {
    Phrase source_phrase = source_phrase_entry.first;
    for (auto target_phrase_entry: source_phrase_entry.second) {
      Phrase target_phrase = target_phrase_entry.first;

      int max_locations = 0, num_locations = 0;
      PhraseAlignment most_frequent_alignment;
      for (auto alignment_entry: target_phrase_entry.second) {
        num_locations += alignment_entry.second;
        if (alignment_entry.second > max_locations) {
          most_frequent_alignment = alignment_entry.first;
          max_locations = alignment_entry.second;
        }
      }

      FeatureContext context(source_phrase, target_phrase,
          source_phrase_counter[source_phrase], num_locations);
      vector<double> scores = scorer->Score(context);
      rules.push_back(Rule(source_phrase, target_phrase, scores,
                           most_frequent_alignment));
    }
  }
  return rules;
}

vector<Extract> RuleExtractor::ExtractAlignments(
    const Phrase& phrase, const vector<int>& matching) const {
  vector<Extract> extracts;
  int sentence_id = source_data_array->GetSentenceId(matching[0]);
  int source_sent_start = source_data_array->GetSentenceStart(sentence_id);

  vector<int> source_low, source_high, target_low, target_high;
  GetLinksSpans(source_low, source_high, target_low, target_high, sentence_id);

  int num_subpatterns = matching.size();
  vector<int> chunklen(num_subpatterns);
  for (size_t i = 0; i < num_subpatterns; ++i) {
    chunklen[i] = phrase.GetChunkLen(i);
  }

  if (!CheckAlignedTerminals(matching, chunklen, source_low) ||
      !CheckTightPhrases(matching, chunklen, source_low)) {
    return extracts;
  }

  int source_back_low = -1, source_back_high = -1;
  int source_phrase_low = matching[0] - source_sent_start;
  int source_phrase_high = matching.back() + chunklen.back() - source_sent_start;
  int target_phrase_low = -1, target_phrase_high = -1;
  if (!FindFixPoint(source_phrase_low, source_phrase_high, source_low,
                    source_high, target_phrase_low, target_phrase_high,
                    target_low, target_high, source_back_low, source_back_high,
                    sentence_id, min_gap_size, 0,
                    max_nonterminals - matching.size() + 1, 1, 1, false)) {
    return extracts;
  }

  bool met_constraints = true;
  int num_symbols = phrase.GetNumSymbols();
  vector<pair<int, int> > source_gaps, target_gaps;
  if (!CheckGaps(source_gaps, target_gaps, matching, chunklen, source_low,
                 source_high, target_low, target_high, source_phrase_low,
                 source_phrase_high, source_back_low, source_back_high,
                 num_symbols, met_constraints)) {
    return extracts;
  }

  bool start_x = source_back_low != source_phrase_low;
  bool end_x = source_back_high != source_phrase_high;
  Phrase source_phrase = phrase_builder->Extend(phrase, start_x, end_x);
  if (met_constraints) {
    AddExtracts(extracts, source_phrase, target_gaps, target_low,
                target_phrase_low, target_phrase_high, sentence_id);
  }

  if (source_gaps.size() >= max_nonterminals ||
      source_phrase.GetNumSymbols() >= max_rule_symbols ||
      source_back_high - source_back_low + min_gap_size > max_rule_span) {
    // Cannot add any more nonterminals.
    return extracts;
  }

  for (int i = 0; i < 2; ++i) {
    for (int j = 1 - i; j < 2; ++j) {
      AddNonterminalExtremities(extracts, source_phrase, source_phrase_low,
          source_phrase_high, source_back_low, source_back_high, source_low,
          source_high, target_low, target_high, target_gaps, sentence_id, i, j);
    }
  }

  return extracts;
}

void RuleExtractor::GetLinksSpans(
    vector<int>& source_low, vector<int>& source_high,
    vector<int>& target_low, vector<int>& target_high, int sentence_id) const {
  // Ignore end of line markers.
  int source_sent_len = source_data_array->GetSentenceStart(sentence_id + 1) -
      source_data_array->GetSentenceStart(sentence_id) - 1;
  int target_sent_len = target_data_array->GetSentenceStart(sentence_id + 1) -
      target_data_array->GetSentenceStart(sentence_id) - 1;
  source_low = vector<int>(source_sent_len, -1);
  source_high = vector<int>(source_sent_len, -1);

  // TODO(pauldb): Adam Lopez claims this part is really inefficient. See if we
  // can speed it up.
  target_low = vector<int>(target_sent_len, -1);
  target_high = vector<int>(target_sent_len, -1);
  const vector<pair<int, int> >& links = alignment->GetLinks(sentence_id);
  for (auto link: links) {
    if (source_low[link.first] == -1 || source_low[link.first] > link.second) {
      source_low[link.first] = link.second;
    }
    source_high[link.first] = max(source_high[link.first], link.second + 1);

    if (target_low[link.second] == -1 || target_low[link.second] > link.first) {
      target_low[link.second] = link.first;
    }
    target_high[link.second] = max(target_high[link.second], link.first + 1);
  }
}

bool RuleExtractor::CheckAlignedTerminals(const vector<int>& matching,
                                          const vector<int>& chunklen,
                                          const vector<int>& source_low) const {
  if (!require_aligned_terminal) {
    return true;
  }

  int sentence_id = source_data_array->GetSentenceId(matching[0]);
  int source_sent_start = source_data_array->GetSentenceStart(sentence_id);

  int num_aligned_chunks = 0;
  for (size_t i = 0; i < chunklen.size(); ++i) {
    for (size_t j = 0; j < chunklen[i]; ++j) {
      int sent_index = matching[i] - source_sent_start + j;
      if (source_low[sent_index] != -1) {
        ++num_aligned_chunks;
        break;
      }
    }
  }

  if (num_aligned_chunks == 0) {
    return false;
  }

  return !require_aligned_chunks || num_aligned_chunks == chunklen.size();
}

bool RuleExtractor::CheckTightPhrases(const vector<int>& matching,
                                      const vector<int>& chunklen,
                                      const vector<int>& source_low) const {
  if (!require_tight_phrases) {
    return true;
  }

  int sentence_id = source_data_array->GetSentenceId(matching[0]);
  int source_sent_start = source_data_array->GetSentenceStart(sentence_id);
  for (size_t i = 0; i + 1 < chunklen.size(); ++i) {
    int gap_start = matching[i] + chunklen[i] - source_sent_start;
    int gap_end = matching[i + 1] - 1 - source_sent_start;
    if (source_low[gap_start] == -1 || source_low[gap_end] == -1) {
      return false;
    }
  }

  return true;
}

bool RuleExtractor::FindFixPoint(
    int source_phrase_low, int source_phrase_high,
    const vector<int>& source_low, const vector<int>& source_high,
    int& target_phrase_low, int& target_phrase_high,
    const vector<int>& target_low, const vector<int>& target_high,
    int& source_back_low, int& source_back_high, int sentence_id,
    int min_source_gap_size, int min_target_gap_size,
    int max_new_x, int max_low_x, int max_high_x,
    bool allow_arbitrary_expansion) const {
  int source_sent_len = source_data_array->GetSentenceStart(sentence_id + 1) -
      source_data_array->GetSentenceStart(sentence_id) - 1;
  int target_sent_len = target_data_array->GetSentenceStart(sentence_id + 1) -
      target_data_array->GetSentenceStart(sentence_id) - 1;

  int prev_target_low = target_phrase_low;
  int prev_target_high = target_phrase_high;
  FindProjection(source_phrase_low, source_phrase_high, source_low,
                 source_high, target_phrase_low, target_phrase_high);

  if (target_phrase_low == -1) {
    // TODO(pauldb): Low priority corner case inherited from Adam's code:
    // If w is unaligned, but we don't require aligned terminals, returning an
    // error here prevents the extraction of the allowed rule
    // X -> X_1 w X_2 / X_1 X_2
    return false;
  }

  if (prev_target_low != -1 && target_phrase_low != prev_target_low) {
    if (prev_target_low - target_phrase_low < min_target_gap_size) {
      target_phrase_low = prev_target_low - min_target_gap_size;
      if (target_phrase_low < 0) {
        return false;
      }
    }
  }

  if (prev_target_high != -1 && target_phrase_high != prev_target_high) {
    if (target_phrase_high - prev_target_high < min_target_gap_size) {
      target_phrase_high = prev_target_high + min_target_gap_size;
      if (target_phrase_high > target_sent_len) {
        return false;
      }
    }
  }

  if (target_phrase_high - target_phrase_low > max_rule_span) {
    return false;
  }

  source_back_low = source_back_high = -1;
  FindProjection(target_phrase_low, target_phrase_high, target_low, target_high,
                 source_back_low, source_back_high);
  int new_x = 0, new_low_x = 0, new_high_x = 0;

  while (true) {
    source_back_low = min(source_back_low, source_phrase_low);
    source_back_high = max(source_back_high, source_phrase_high);

    if (source_back_low == source_phrase_low &&
        source_back_high == source_phrase_high) {
      return true;
    }

    if (new_low_x >= max_low_x && source_back_low < source_phrase_low) {
      // Extension on the left side not allowed.
      return false;
    }
    if (new_high_x >= max_high_x && source_back_high > source_phrase_high) {
      // Extension on the right side not allowed.
      return false;
    }

    // Extend left side.
    if (source_back_low < source_phrase_low) {
      if (new_x >= max_new_x) {
        return false;
      }
      ++new_x; ++new_low_x;
      if (source_phrase_low - source_back_low < min_source_gap_size) {
        source_back_low = source_phrase_low - min_source_gap_size;
        if (source_back_low < 0) {
          return false;
        }
      }
    }

    // Extend right side.
    if (source_back_high > source_phrase_high) {
      if (new_x >= max_new_x) {
        return false;
      }
      ++new_x; ++new_high_x;
      if (source_back_high - source_phrase_high < min_source_gap_size) {
        source_back_high = source_phrase_high + min_source_gap_size;
        if (source_back_high > source_sent_len) {
          return false;
        }
      }
    }

    if (source_back_high - source_back_low > max_rule_span) {
      // Rule span too wide.
      return false;
    }

    prev_target_low = target_phrase_low;
    prev_target_high = target_phrase_high;
    FindProjection(source_back_low, source_phrase_low, source_low, source_high,
                   target_phrase_low, target_phrase_high);
    FindProjection(source_phrase_high, source_back_high, source_low,
                   source_high, target_phrase_low, target_phrase_high);
    if (prev_target_low == target_phrase_low &&
        prev_target_high == target_phrase_high) {
      return true;
    }

    if (!allow_arbitrary_expansion) {
      // Arbitrary expansion not allowed.
      return false;
    }
    if (target_phrase_high - target_phrase_low > max_rule_span) {
      // Target side too wide.
      return false;
    }

    source_phrase_low = source_back_low;
    source_phrase_high = source_back_high;
    FindProjection(target_phrase_low, prev_target_low, target_low, target_high,
                   source_back_low, source_back_high);
    FindProjection(prev_target_high, target_phrase_high, target_low,
                   target_high, source_back_low, source_back_high);
  }

  return false;
}

void RuleExtractor::FindProjection(
    int source_phrase_low, int source_phrase_high,
    const vector<int>& source_low, const vector<int>& source_high,
    int& target_phrase_low, int& target_phrase_high) const {
  for (size_t i = source_phrase_low; i < source_phrase_high; ++i) {
    if (source_low[i] != -1) {
      if (target_phrase_low == -1 || source_low[i] < target_phrase_low) {
        target_phrase_low = source_low[i];
      }
      target_phrase_high = max(target_phrase_high, source_high[i]);
    }
  }
}

bool RuleExtractor::CheckGaps(
     vector<pair<int, int> >& source_gaps, vector<pair<int, int> >& target_gaps,
     const vector<int>& matching, const vector<int>& chunklen,
     const vector<int>& source_low, const vector<int>& source_high,
     const vector<int>& target_low, const vector<int>& target_high,
     int source_phrase_low, int source_phrase_high, int source_back_low,
     int source_back_high, int& num_symbols, bool& met_constraints) const {
  int sentence_id = source_data_array->GetSentenceId(matching[0]);
  int source_sent_start = source_data_array->GetSentenceStart(sentence_id);

  if (source_back_low < source_phrase_low) {
    source_gaps.push_back(make_pair(source_back_low, source_phrase_low));
    if (num_symbols >= max_rule_symbols) {
      // Source side contains too many symbols.
      return false;
    }
    ++num_symbols;
    if (require_tight_phrases && (source_low[source_back_low] == -1 ||
        source_low[source_phrase_low - 1] == -1)) {
      // Inside edges of preceding gap are not tight.
      return false;
    }
  } else if (require_tight_phrases && source_low[source_phrase_low] == -1) {
    // This is not a hard error. We can't extract this phrase, but we might
    // still be able to extract a superphrase.
    met_constraints = false;
  }

  for (size_t i = 0; i + 1 < chunklen.size(); ++i) {
    int gap_start = matching[i] + chunklen[i] - source_sent_start;
    int gap_end = matching[i + 1] - source_sent_start;
    source_gaps.push_back(make_pair(gap_start, gap_end));
  }

  if (source_phrase_high < source_back_high) {
    source_gaps.push_back(make_pair(source_phrase_high, source_back_high));
    if (num_symbols >= max_rule_symbols) {
      // Source side contains too many symbols.
      return false;
    }
    ++num_symbols;
    if (require_tight_phrases && (source_low[source_phrase_high] == -1 ||
        source_low[source_back_high - 1] == -1)) {
      // Inside edges of following gap are not tight.
      return false;
    }
  } else if (require_tight_phrases &&
             source_low[source_phrase_high - 1] == -1) {
    // This is not a hard error. We can't extract this phrase, but we might
    // still be able to extract a superphrase.
    met_constraints = false;
  }

  target_gaps.resize(source_gaps.size(), make_pair(-1, -1));
  for (size_t i = 0; i < source_gaps.size(); ++i) {
    if (!FindFixPoint(source_gaps[i].first, source_gaps[i].second, source_low,
                      source_high, target_gaps[i].first, target_gaps[i].second,
                      target_low, target_high, source_gaps[i].first,
                      source_gaps[i].second, sentence_id, 0, 0, 0, 0, 0,
                      false)) {
      // Gap fails integrity check.
      return false;
    }
  }

  return true;
}

void RuleExtractor::AddExtracts(
    vector<Extract>& extracts, const Phrase& source_phrase,
    const vector<pair<int, int> >& target_gaps, const vector<int>& target_low,
    int target_phrase_low, int target_phrase_high, int sentence_id) const {
  vector<pair<Phrase, PhraseAlignment> > target_phrases = ExtractTargetPhrases(
      target_gaps, target_low, target_phrase_low, target_phrase_high,
      sentence_id);

  if (target_phrases.size() > 0) {
    double pairs_count = 1.0 / target_phrases.size();
    for (auto target_phrase: target_phrases) {
      extracts.push_back(Extract(source_phrase, target_phrase.first,
                                 pairs_count, target_phrase.second));
    }
  }
}

vector<pair<Phrase, PhraseAlignment> > RuleExtractor::ExtractTargetPhrases(
    const vector<pair<int, int> >& target_gaps, const vector<int>& target_low,
    int target_phrase_low, int target_phrase_high, int sentence_id) const {
  int target_sent_len = target_data_array->GetSentenceStart(sentence_id + 1) -
      target_data_array->GetSentenceStart(sentence_id) - 1;

  vector<int> target_gap_order(target_gaps.size());
  for (size_t i = 0; i < target_gap_order.size(); ++i) {
    for (size_t j = 0; j < i; ++j) {
      if (target_gaps[target_gap_order[j]] < target_gaps[i]) {
        ++target_gap_order[i];
      } else {
        ++target_gap_order[j];
      }
    }
  }

  int target_x_low = target_phrase_low, target_x_high = target_phrase_high;
  if (!require_tight_phrases) {
    while (target_x_low > 0 &&
           target_phrase_high - target_x_low < max_rule_span &&
           target_low[target_x_low - 1] == -1) {
      --target_x_low;
    }
    while (target_x_high + 1 < target_sent_len &&
           target_x_high - target_phrase_low < max_rule_span &&
           target_low[target_x_high + 1] == -1) {
      ++target_x_high;
    }
  }

  vector<pair<int, int> > gaps(target_gaps.size());
  for (size_t i = 0; i < gaps.size(); ++i) {
    gaps[i] = target_gaps[target_gap_order[i]];
    if (!require_tight_phrases) {
      while (gaps[i].first > target_x_low &&
             target_low[gaps[i].first] == -1) {
        --gaps[i].first;
      }
      while (gaps[i].second < target_x_high &&
             target_low[gaps[i].second] == -1) {
        ++gaps[i].second;
      }
    }
  }

  vector<pair<int, int> > ranges(2 * gaps.size() + 2);
  ranges.front() = make_pair(target_x_low, target_phrase_low);
  ranges.back() = make_pair(target_phrase_high, target_x_high);
  for (size_t i = 0; i < gaps.size(); ++i) {
    ranges[i * 2 + 1] = make_pair(gaps[i].first, target_gaps[i].first);
    ranges[i * 2 + 2] = make_pair(target_gaps[i].second, gaps[i].second);
  }

  vector<pair<Phrase, PhraseAlignment> > target_phrases;
  vector<int> subpatterns(ranges.size());
  GeneratePhrases(target_phrases, ranges, 0, subpatterns, target_gap_order,
                  target_phrase_low, target_phrase_high, sentence_id);
  return target_phrases;
}

void RuleExtractor::GeneratePhrases(
    vector<pair<Phrase, PhraseAlignment> >& target_phrases,
    const vector<pair<int, int> >& ranges, int index, vector<int>& subpatterns,
    const vector<int>& target_gap_order, int target_phrase_low,
    int target_phrase_high, int sentence_id) const {
  if (index >= ranges.size()) {
    if (subpatterns.back() - subpatterns.front() > max_rule_span) {
      return;
    }

    vector<int> symbols;
    unordered_set<int> target_indexes;
    int offset = 1;
    if (subpatterns.front() != target_phrase_low) {
      offset = 2;
      symbols.push_back(vocabulary->GetNonterminalIndex(1));
    }

    int target_sent_start = target_data_array->GetSentenceStart(sentence_id);
    for (size_t i = 0; i * 2 < subpatterns.size(); ++i) {
      for (size_t j = subpatterns[i * 2]; j < subpatterns[i * 2 + 1]; ++j) {
        symbols.push_back(target_data_array->AtIndex(target_sent_start + j));
        target_indexes.insert(j);
      }
      if (i < target_gap_order.size()) {
        symbols.push_back(vocabulary->GetNonterminalIndex(
            target_gap_order[i] + offset));
      }
    }

    if (subpatterns.back() != target_phrase_high) {
      symbols.push_back(target_gap_order.size() + offset);
    }

    const vector<pair<int, int> >& links = alignment->GetLinks(sentence_id);
    vector<pair<int, int> > alignment;
    for (pair<int, int> link: links) {
      if (target_indexes.count(link.second)) {
        alignment.push_back(link);
      }
    }

    target_phrases.push_back(make_pair(phrase_builder->Build(symbols),
                                       alignment));
    return;
  }

  subpatterns[index] = ranges[index].first;
  if (index > 0) {
    subpatterns[index] = max(subpatterns[index], subpatterns[index - 1]);
  }
  while (subpatterns[index] <= ranges[index].second) {
    subpatterns[index + 1] = max(subpatterns[index], ranges[index + 1].first);
    while (subpatterns[index + 1] <= ranges[index + 1].second) {
      GeneratePhrases(target_phrases, ranges, index + 2, subpatterns,
                      target_gap_order, target_phrase_low, target_phrase_high,
                      sentence_id);
      ++subpatterns[index + 1];
    }
    ++subpatterns[index];
  }
}

void RuleExtractor::AddNonterminalExtremities(
    vector<Extract>& extracts, const Phrase& source_phrase,
    int source_phrase_low, int source_phrase_high, int source_back_low,
    int source_back_high, const vector<int>& source_low,
    const vector<int>& source_high, const vector<int>& target_low,
    const vector<int>& target_high, const vector<pair<int, int> >& target_gaps,
    int sentence_id, int extend_left, int extend_right) const {
  int source_x_low = source_phrase_low, source_x_high = source_phrase_high;
  if (extend_left) {
    if (source_back_low != source_phrase_low ||
        source_phrase_low < min_gap_size ||
        (require_tight_phrases && (source_low[source_phrase_low - 1] == -1 ||
                                   source_low[source_back_high - 1] == -1))) {
      return;
    }

    source_x_low = source_phrase_low - min_gap_size;
    if (require_tight_phrases) {
      while (source_x_low >= 0 && source_low[source_x_low] == -1) {
        --source_x_low;
      }
    }
    if (source_x_low < 0) {
      return;
    }
  }

  if (extend_right) {
    int source_sent_len = source_data_array->GetSentenceStart(sentence_id + 1) -
        source_data_array->GetSentenceStart(sentence_id) - 1;
    if (source_back_high != source_phrase_high ||
        source_phrase_high + min_gap_size > source_sent_len ||
        (require_tight_phrases && (source_low[source_phrase_low] == -1 ||
                                   source_low[source_phrase_high] == -1))) {
      return;
    }
    source_x_high = source_phrase_high + min_gap_size;
    if (require_tight_phrases) {
      while (source_x_high <= source_sent_len &&
             source_low[source_x_high - 1] == -1) {
        ++source_x_high;
      }
    }

    if (source_x_high > source_sent_len) {
      return;
    }
  }

  if (source_x_high - source_x_low > max_rule_span ||
      target_gaps.size() + extend_left + extend_right > max_nonterminals) {
    return;
  }

  int target_x_low = -1, target_x_high = -1;
  if (!FindFixPoint(source_x_low, source_x_high, source_low, source_high,
                    target_x_low, target_x_high, target_low, target_high,
                    source_x_low, source_x_high, sentence_id, 1, 1,
                    extend_left + extend_right, extend_left, extend_right,
                    true)) {
    return;
  }

  int source_gap_low = -1, source_gap_high = -1, target_gap_low = -1,
      target_gap_high = -1;
  if (extend_left &&
      ((require_tight_phrases && source_low[source_x_low] == -1) ||
       !FindFixPoint(source_x_low, source_phrase_low, source_low, source_high,
                     target_gap_low, target_gap_high, target_low, target_high,
                     source_gap_low, source_gap_high, sentence_id,
                     0, 0, 0, 0, 0, false))) {
    return;
  }
  if (extend_right &&
      ((require_tight_phrases && source_low[source_x_high - 1] == -1) ||
       !FindFixPoint(source_phrase_high, source_x_high, source_low, source_high,
                     target_gap_low, target_gap_high, target_low, target_high,
                     source_gap_low, source_gap_high, sentence_id,
                     0, 0, 0, 0, 0, false))) {
    return;
  }

  Phrase new_source_phrase = phrase_builder->Extend(source_phrase, extend_left,
                                                    extend_right);
  AddExtracts(extracts, new_source_phrase, target_gaps, target_low,
              target_x_low, target_x_high, sentence_id);
}
