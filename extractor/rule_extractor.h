#ifndef _RULE_EXTRACTOR_H_
#define _RULE_EXTRACTOR_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "phrase.h"

using namespace std;

namespace extractor {

typedef vector<pair<int, int> > PhraseAlignment;

class Alignment;
class DataArray;
class PhraseBuilder;
class PhraseLocation;
class Rule;
class RuleExtractorHelper;
class Scorer;
class TargetPhraseExtractor;

struct Extract {
  Extract(const Phrase& source_phrase, const Phrase& target_phrase,
          double pairs_count, const PhraseAlignment& alignment) :
      source_phrase(source_phrase), target_phrase(target_phrase),
      pairs_count(pairs_count), alignment(alignment) {}

  Phrase source_phrase;
  Phrase target_phrase;
  double pairs_count;
  PhraseAlignment alignment;
};

class RuleExtractor {
 public:
  RuleExtractor(shared_ptr<DataArray> source_data_array,
                shared_ptr<DataArray> target_data_array,
                shared_ptr<Alignment> alingment,
                shared_ptr<PhraseBuilder> phrase_builder,
                shared_ptr<Scorer> scorer,
                shared_ptr<Vocabulary> vocabulary,
                int min_gap_size,
                int max_rule_span,
                int max_nonterminals,
                int max_rule_symbols,
                bool require_aligned_terminal,
                bool require_aligned_chunks,
                bool require_tight_phrases);

  // For testing only.
  RuleExtractor(shared_ptr<DataArray> source_data_array,
                shared_ptr<PhraseBuilder> phrase_builder,
                shared_ptr<Scorer> scorer,
                shared_ptr<TargetPhraseExtractor> target_phrase_extractor,
                shared_ptr<RuleExtractorHelper> helper,
                int max_rule_span,
                int min_gap_size,
                int max_nonterminals,
                int max_rule_symbols,
                bool require_tight_phrases);

  virtual ~RuleExtractor();

  virtual vector<Rule> ExtractRules(const Phrase& phrase,
                                    const PhraseLocation& location) const;

 protected:
  RuleExtractor();

 private:
  vector<Extract> ExtractAlignments(const Phrase& phrase,
                                    const vector<int>& matching) const;

  void AddExtracts(
      vector<Extract>& extracts, const Phrase& source_phrase,
      const unordered_map<int, int>& source_indexes,
      const vector<pair<int, int> >& target_gaps, const vector<int>& target_low,
      int target_phrase_low, int target_phrase_high, int sentence_id) const;

  void AddNonterminalExtremities(
      vector<Extract>& extracts, const vector<int>& matching,
      const vector<int>& chunklen, const Phrase& source_phrase,
      int source_back_low, int source_back_high, const vector<int>& source_low,
      const vector<int>& source_high, const vector<int>& target_low,
      const vector<int>& target_high, vector<pair<int, int> > target_gaps,
      int sentence_id, int starts_with_x, int ends_with_x, int extend_left,
      int extend_right) const;

 private:
  shared_ptr<DataArray> target_data_array;
  shared_ptr<DataArray> source_data_array;
  shared_ptr<PhraseBuilder> phrase_builder;
  shared_ptr<Scorer> scorer;
  shared_ptr<TargetPhraseExtractor> target_phrase_extractor;
  shared_ptr<RuleExtractorHelper> helper;
  int max_rule_span;
  int min_gap_size;
  int max_nonterminals;
  int max_rule_symbols;
  bool require_tight_phrases;
};

} // namespace extractor

#endif
