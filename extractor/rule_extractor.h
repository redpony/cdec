#ifndef _RULE_EXTRACTOR_H_
#define _RULE_EXTRACTOR_H_

#include <memory>
#include <vector>

#include "phrase.h"

using namespace std;

class Alignment;
class DataArray;
class PhraseBuilder;
class PhraseLocation;
class Rule;
class Scorer;
class Vocabulary;

typedef vector<pair<int, int> > PhraseAlignment;

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

  vector<Rule> ExtractRules(const Phrase& phrase,
                            const PhraseLocation& location) const;

 private:
  vector<Extract> ExtractAlignments(const Phrase& phrase,
                                    const vector<int>& matching) const;

  void GetLinksSpans(vector<int>& source_low, vector<int>& source_high,
                     vector<int>& target_low, vector<int>& target_high,
                     int sentence_id) const;

  bool CheckAlignedTerminals(const vector<int>& matching,
                             const vector<int>& chunklen,
                             const vector<int>& source_low) const;

  bool CheckTightPhrases(const vector<int>& matching,
                         const vector<int>& chunklen,
                         const vector<int>& source_low) const;

  bool FindFixPoint(
      int source_phrase_start, int source_phrase_end,
      const vector<int>& source_low, const vector<int>& source_high,
      int& target_phrase_start, int& target_phrase_end,
      const vector<int>& target_low, const vector<int>& target_high,
      int& source_back_low, int& source_back_high, int sentence_id,
      int min_source_gap_size, int min_target_gap_size,
      int max_new_x, int max_low_x, int max_high_x,
      bool allow_arbitrary_expansion) const;

  void FindProjection(
      int source_phrase_start, int source_phrase_end,
      const vector<int>& source_low, const vector<int>& source_high,
      int& target_phrase_low, int& target_phrase_end) const;

  bool CheckGaps(
     vector<pair<int, int> >& source_gaps, vector<pair<int, int> >& target_gaps,
     const vector<int>& matching, const vector<int>& chunklen,
     const vector<int>& source_low, const vector<int>& source_high,
     const vector<int>& target_low, const vector<int>& target_high,
     int source_phrase_low, int source_phrase_high, int source_back_low,
     int source_back_high, int& num_symbols, bool& met_constraints) const;

  void AddExtracts(
      vector<Extract>& extracts, const Phrase& source_phrase,
      const vector<pair<int, int> >& target_gaps, const vector<int>& target_low,
      int target_phrase_low, int target_phrase_high, int sentence_id) const;

  vector<pair<Phrase, PhraseAlignment> > ExtractTargetPhrases(
      const vector<pair<int, int> >& target_gaps, const vector<int>& target_low,
      int target_phrase_low, int target_phrase_high, int sentence_id) const;

  void GeneratePhrases(
      vector<pair<Phrase, PhraseAlignment> >& target_phrases,
      const vector<pair<int, int> >& ranges, int index,
      vector<int>& subpatterns, const vector<int>& target_gap_order,
      int target_phrase_low, int target_phrase_high, int sentence_id) const;

  void AddNonterminalExtremities(
      vector<Extract>& extracts, const Phrase& source_phrase,
      int source_phrase_low, int source_phrase_high, int source_back_low,
      int source_back_high, const vector<int>& source_low,
      const vector<int>& source_high, const vector<int>& target_low,
      const vector<int>& target_high,
      const vector<pair<int, int> >& target_gaps, int sentence_id,
      int extend_left, int extend_right) const;

  shared_ptr<DataArray> source_data_array;
  shared_ptr<DataArray> target_data_array;
  shared_ptr<Alignment> alignment;
  shared_ptr<PhraseBuilder> phrase_builder;
  shared_ptr<Scorer> scorer;
  shared_ptr<Vocabulary> vocabulary;
  int max_rule_span;
  int min_gap_size;
  int max_nonterminals;
  int max_rule_symbols;
  bool require_aligned_terminal;
  bool require_aligned_chunks;
  bool require_tight_phrases;
};

#endif
