#ifndef _RULE_EXTRACTOR_HELPER_H_
#define _RULE_EXTRACTOR_HELPER_H_

#include <memory>
#include <unordered_map>
#include <vector>

using namespace std;

namespace extractor {

class Alignment;
class DataArray;

/**
 * Helper class for extracting SCFG rules.
 */
class RuleExtractorHelper {
 public:
  RuleExtractorHelper(shared_ptr<DataArray> source_data_array,
                      shared_ptr<DataArray> target_data_array,
                      shared_ptr<Alignment> alignment,
                      int max_rule_span,
                      int max_rule_symbols,
                      bool require_aligned_terminal,
                      bool require_aligned_chunks,
                      bool require_tight_phrases);

  virtual ~RuleExtractorHelper();

  // Find the alignment span for each word in the source target sentence pair.
  virtual void GetLinksSpans(vector<int>& source_low, vector<int>& source_high,
                             vector<int>& target_low, vector<int>& target_high,
                             int sentence_id) const;

  // Check if one chunk (all chunks) is aligned at least in one point.
  virtual bool CheckAlignedTerminals(const vector<int>& matching,
                                     const vector<int>& chunklen,
                                     const vector<int>& source_low,
                                     int source_sent_start) const;

  // Check if the chunks are tight.
  virtual bool CheckTightPhrases(const vector<int>& matching,
                                 const vector<int>& chunklen,
                                 const vector<int>& source_low,
                                 int source_sent_start) const;

  // Find the target span and the reflected source span for a source phrase
  // occurrence.
  virtual bool FindFixPoint(
      int source_phrase_low, int source_phrase_high,
      const vector<int>& source_low, const vector<int>& source_high,
      int& target_phrase_low, int& target_phrase_high,
      const vector<int>& target_low, const vector<int>& target_high,
      int& source_back_low, int& source_back_high, int sentence_id,
      int min_source_gap_size, int min_target_gap_size,
      int max_new_x, bool allow_low_x, bool allow_high_x,
      bool allow_arbitrary_expansion) const;

  // Find the gap spans for each nonterminal in the source phrase.
  virtual bool GetGaps(
      vector<pair<int, int>>& source_gaps, vector<pair<int, int>>& target_gaps,
      const vector<int>& matching, const vector<int>& chunklen,
      const vector<int>& source_low, const vector<int>& source_high,
      const vector<int>& target_low, const vector<int>& target_high,
      int source_phrase_low, int source_phrase_high, int source_back_low,
      int source_back_high, int sentence_id, int source_sent_start,
      int& num_symbols, bool& met_constraints) const;

  // Get the order of the nonterminals in the target phrase.
  virtual vector<int> GetGapOrder(const vector<pair<int, int>>& gaps) const;

  // Map each terminal symbol with its position in the source phrase.
  virtual unordered_map<int, int> GetSourceIndexes(
      const vector<int>& matching, const vector<int>& chunklen,
      int starts_with_x, int source_sent_start) const;

 protected:
  RuleExtractorHelper();

 private:
  // Find the projection of a source phrase in the target sentence. May also be
  // used to find the projection of a target phrase in the source sentence.
  void FindProjection(
      int source_phrase_low, int source_phrase_high,
      const vector<int>& source_low, const vector<int>& source_high,
      int& target_phrase_low, int& target_phrase_high) const;

  shared_ptr<DataArray> source_data_array;
  shared_ptr<DataArray> target_data_array;
  shared_ptr<Alignment> alignment;
  int max_rule_span;
  int max_rule_symbols;
  bool require_aligned_terminal;
  bool require_aligned_chunks;
  bool require_tight_phrases;
};

} // namespace extractor

#endif
