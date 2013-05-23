#include "rule_extractor.h"

#include <map>

#include "alignment.h"
#include "data_array.h"
#include "features/feature.h"
#include "phrase_builder.h"
#include "phrase_location.h"
#include "rule.h"
#include "rule_extractor_helper.h"
#include "scorer.h"
#include "target_phrase_extractor.h"

using namespace std;

namespace extractor {

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
    target_data_array(target_data_array),
    source_data_array(source_data_array),
    phrase_builder(phrase_builder),
    scorer(scorer),
    max_rule_span(max_rule_span),
    min_gap_size(min_gap_size),
    max_nonterminals(max_nonterminals),
    max_rule_symbols(max_rule_symbols),
    require_tight_phrases(require_tight_phrases) {
  helper = make_shared<RuleExtractorHelper>(
      source_data_array, target_data_array, alignment, max_rule_span,
      max_rule_symbols, require_aligned_terminal, require_aligned_chunks,
      require_tight_phrases);
  target_phrase_extractor = make_shared<TargetPhraseExtractor>(
      target_data_array, alignment, phrase_builder, helper, vocabulary,
      max_rule_span, require_tight_phrases);
}

RuleExtractor::RuleExtractor(
    shared_ptr<DataArray> source_data_array,
    shared_ptr<PhraseBuilder> phrase_builder,
    shared_ptr<Scorer> scorer,
    shared_ptr<TargetPhraseExtractor> target_phrase_extractor,
    shared_ptr<RuleExtractorHelper> helper,
    int max_rule_span,
    int min_gap_size,
    int max_nonterminals,
    int max_rule_symbols,
    bool require_tight_phrases) :
    source_data_array(source_data_array),
    phrase_builder(phrase_builder),
    scorer(scorer),
    target_phrase_extractor(target_phrase_extractor),
    helper(helper),
    max_rule_span(max_rule_span),
    min_gap_size(min_gap_size),
    max_nonterminals(max_nonterminals),
    max_rule_symbols(max_rule_symbols),
    require_tight_phrases(require_tight_phrases) {}

RuleExtractor::RuleExtractor() {}

RuleExtractor::~RuleExtractor() {}

vector<Rule> RuleExtractor::ExtractRules(const Phrase& phrase,
                                         const PhraseLocation& location) const {
  int num_subpatterns = location.num_subpatterns;
  vector<int> matchings = *location.matchings;

  // Calculate statistics for the (sampled) occurrences of the source phrase.
  map<Phrase, double> source_phrase_counter;
  map<Phrase, map<Phrase, map<PhraseAlignment, int>>> alignments_counter;
  for (auto i = matchings.begin(); i != matchings.end(); i += num_subpatterns) {
    vector<int> matching(i, i + num_subpatterns);
    vector<Extract> extracts = ExtractAlignments(phrase, matching);

    for (Extract e: extracts) {
      source_phrase_counter[e.source_phrase] += e.pairs_count;
      alignments_counter[e.source_phrase][e.target_phrase][e.alignment] += 1;
    }
  }

  // Compute the feature scores and find the most likely (frequent) alignment
  // for each pair of source-target phrases.
  int num_samples = matchings.size() / num_subpatterns;
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

      features::FeatureContext context(source_phrase, target_phrase,
          source_phrase_counter[source_phrase], num_locations, num_samples);
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

  // Get the span in the opposite sentence for each word in the source-target
  // sentece pair.
  vector<int> source_low, source_high, target_low, target_high;
  helper->GetLinksSpans(source_low, source_high, target_low, target_high,
                        sentence_id);

  int num_subpatterns = matching.size();
  vector<int> chunklen(num_subpatterns);
  for (size_t i = 0; i < num_subpatterns; ++i) {
    chunklen[i] = phrase.GetChunkLen(i);
  }

  // Basic checks to see if we can extract phrase pairs for this occurrence.
  if (!helper->CheckAlignedTerminals(matching, chunklen, source_low,
                                     source_sent_start) ||
      !helper->CheckTightPhrases(matching, chunklen, source_low,
                                 source_sent_start)) {
    return extracts;
  }

  int source_back_low = -1, source_back_high = -1;
  int source_phrase_low = matching[0] - source_sent_start;
  int source_phrase_high = matching.back() + chunklen.back() -
                           source_sent_start;
  int target_phrase_low = -1, target_phrase_high = -1;
  // Find target span and reflected source span for the source phrase.
  if (!helper->FindFixPoint(source_phrase_low, source_phrase_high, source_low,
                            source_high, target_phrase_low, target_phrase_high,
                            target_low, target_high, source_back_low,
                            source_back_high, sentence_id, min_gap_size, 0,
                            max_nonterminals - matching.size() + 1, true, true,
                            false)) {
    return extracts;
  }

  // Get spans for nonterminal gaps.
  bool met_constraints = true;
  int num_symbols = phrase.GetNumSymbols();
  vector<pair<int, int>> source_gaps, target_gaps;
  if (!helper->GetGaps(source_gaps, target_gaps, matching, chunklen, source_low,
                       source_high, target_low, target_high, source_phrase_low,
                       source_phrase_high, source_back_low, source_back_high,
                       sentence_id, source_sent_start, num_symbols,
                       met_constraints)) {
    return extracts;
  }

  // Find target phrases aligned with the initial source phrase.
  bool starts_with_x = source_back_low != source_phrase_low;
  bool ends_with_x = source_back_high != source_phrase_high;
  Phrase source_phrase = phrase_builder->Extend(
      phrase, starts_with_x, ends_with_x);
  unordered_map<int, int> source_indexes = helper->GetSourceIndexes(
      matching, chunklen, starts_with_x, source_sent_start);
  if (met_constraints) {
    AddExtracts(extracts, source_phrase, source_indexes, target_gaps,
                target_low, target_phrase_low, target_phrase_high, sentence_id);
  }

  if (source_gaps.size() >= max_nonterminals ||
      source_phrase.GetNumSymbols() >= max_rule_symbols ||
      source_back_high - source_back_low + min_gap_size > max_rule_span) {
    // Cannot add any more nonterminals.
    return extracts;
  }

  // Extend the source phrase by adding a leading and/or trailing nonterminal
  // and find target phrases aligned with the extended source phrase.
  for (int i = 0; i < 2; ++i) {
    for (int j = 1 - i; j < 2; ++j) {
      AddNonterminalExtremities(extracts, matching, chunklen, source_phrase,
          source_back_low, source_back_high, source_low, source_high,
          target_low, target_high, target_gaps, sentence_id, source_sent_start,
          starts_with_x, ends_with_x, i, j);
    }
  }

  return extracts;
}

void RuleExtractor::AddExtracts(
    vector<Extract>& extracts, const Phrase& source_phrase,
    const unordered_map<int, int>& source_indexes,
    const vector<pair<int, int>>& target_gaps, const vector<int>& target_low,
    int target_phrase_low, int target_phrase_high, int sentence_id) const {
  auto target_phrases = target_phrase_extractor->ExtractPhrases(
      target_gaps, target_low, target_phrase_low, target_phrase_high,
      source_indexes, sentence_id);

  if (target_phrases.size() > 0) {
    // Split the probability equally across all target phrases that can be
    // aligned with a single occurrence of the source phrase.
    double pairs_count = 1.0 / target_phrases.size();
    for (auto target_phrase: target_phrases) {
      extracts.push_back(Extract(source_phrase, target_phrase.first,
                                 pairs_count, target_phrase.second));
    }
  }
}

void RuleExtractor::AddNonterminalExtremities(
    vector<Extract>& extracts, const vector<int>& matching,
    const vector<int>& chunklen, const Phrase& source_phrase,
    int source_back_low, int source_back_high, const vector<int>& source_low,
    const vector<int>& source_high, const vector<int>& target_low,
    const vector<int>& target_high, vector<pair<int, int>> target_gaps,
    int sentence_id, int source_sent_start, int starts_with_x, int ends_with_x,
    int extend_left, int extend_right) const {
  int source_x_low = source_back_low, source_x_high = source_back_high;

  // Check if the extended source phrase will remain tight.
  if (require_tight_phrases) {
    if (source_low[source_back_low - extend_left] == -1 ||
        source_low[source_back_high + extend_right - 1] == -1) {
      return;
    }
  }

  // Check if we can add a nonterminal to the left.
  if (extend_left) {
    if (starts_with_x || source_back_low < min_gap_size) {
      return;
    }

    source_x_low = source_back_low - min_gap_size;
    if (require_tight_phrases) {
      while (source_x_low >= 0 && source_low[source_x_low] == -1) {
        --source_x_low;
      }
    }
    if (source_x_low < 0) {
      return;
    }
  }

  // Check if we can add a nonterminal to the right.
  if (extend_right) {
    int source_sent_len = source_data_array->GetSentenceLength(sentence_id);
    if (ends_with_x || source_back_high + min_gap_size > source_sent_len) {
      return;
    }
    source_x_high = source_back_high + min_gap_size;
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

  // More length checks.
  int new_nonterminals = extend_left + extend_right;
  if (source_x_high - source_x_low > max_rule_span ||
      target_gaps.size() + new_nonterminals > max_nonterminals ||
      source_phrase.GetNumSymbols() + new_nonterminals > max_rule_symbols) {
    return;
  }

  // Find the target span for the extended phrase and the reflected source span.
  int target_x_low = -1, target_x_high = -1;
  if (!helper->FindFixPoint(source_x_low, source_x_high, source_low,
                            source_high, target_x_low, target_x_high,
                            target_low, target_high, source_x_low,
                            source_x_high, sentence_id, 1, 1,
                            new_nonterminals, extend_left, extend_right,
                            true)) {
    return;
  }

  // Check gap integrity for the leading nonterminal.
  if (extend_left) {
    int source_gap_low = -1, source_gap_high = -1;
    int target_gap_low = -1, target_gap_high = -1;
    if ((require_tight_phrases && source_low[source_x_low] == -1) ||
        !helper->FindFixPoint(source_x_low, source_back_low, source_low,
                              source_high, target_gap_low, target_gap_high,
                              target_low, target_high, source_gap_low,
                              source_gap_high, sentence_id, 0, 0, 0, false,
                              false, false)) {
      return;
    }
    target_gaps.insert(target_gaps.begin(),
                       make_pair(target_gap_low, target_gap_high));
  }

  // Check gap integrity for the trailing nonterminal.
  if (extend_right) {
    int target_gap_low = -1, target_gap_high = -1;
    int source_gap_low = -1, source_gap_high = -1;
    if ((require_tight_phrases && source_low[source_x_high - 1] == -1) ||
        !helper->FindFixPoint(source_back_high, source_x_high, source_low,
                              source_high, target_gap_low, target_gap_high,
                              target_low, target_high, source_gap_low,
                              source_gap_high, sentence_id, 0, 0, 0, false,
                              false, false)) {
      return;
    }
    target_gaps.push_back(make_pair(target_gap_low, target_gap_high));
  }

  // Find target phrases aligned with the extended source phrase.
  Phrase new_source_phrase = phrase_builder->Extend(source_phrase, extend_left,
                                                    extend_right);
  unordered_map<int, int> source_indexes = helper->GetSourceIndexes(
      matching, chunklen, extend_left || starts_with_x, source_sent_start);
  AddExtracts(extracts, new_source_phrase, source_indexes, target_gaps,
              target_low, target_x_low, target_x_high, sentence_id);
}

} // namespace extractor
