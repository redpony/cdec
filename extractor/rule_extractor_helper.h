#ifndef _RULE_EXTRACTOR_HELPER_H_
#define _RULE_EXTRACTOR_HELPER_H_

#include <memory>
#include <unordered_map>
#include <vector>

using namespace std;

class Alignment;
class DataArray;

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

  virtual void GetLinksSpans(vector<int>& source_low, vector<int>& source_high,
                             vector<int>& target_low, vector<int>& target_high,
                             int sentence_id) const;

  virtual bool CheckAlignedTerminals(const vector<int>& matching,
                                     const vector<int>& chunklen,
                                     const vector<int>& source_low) const;

  virtual bool CheckTightPhrases(const vector<int>& matching,
                                 const vector<int>& chunklen,
                                 const vector<int>& source_low) const;

  virtual bool FindFixPoint(
      int source_phrase_low, int source_phrase_high,
      const vector<int>& source_low, const vector<int>& source_high,
      int& target_phrase_low, int& target_phrase_high,
      const vector<int>& target_low, const vector<int>& target_high,
      int& source_back_low, int& source_back_high, int sentence_id,
      int min_source_gap_size, int min_target_gap_size,
      int max_new_x, bool allow_low_x, bool allow_high_x,
      bool allow_arbitrary_expansion) const;

  virtual bool GetGaps(
      vector<pair<int, int> >& source_gaps, vector<pair<int, int> >& target_gaps,
      const vector<int>& matching, const vector<int>& chunklen,
      const vector<int>& source_low, const vector<int>& source_high,
      const vector<int>& target_low, const vector<int>& target_high,
      int source_phrase_low, int source_phrase_high, int source_back_low,
      int source_back_high, int& num_symbols, bool& met_constraints) const;

  virtual vector<int> GetGapOrder(const vector<pair<int, int> >& gaps) const;

  // TODO(pauldb): Add unit tests.
  virtual unordered_map<int, int> GetSourceIndexes(
      const vector<int>& matching, const vector<int>& chunklen,
      int starts_with_x) const;

 protected:
  RuleExtractorHelper();

 private:
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

#endif
