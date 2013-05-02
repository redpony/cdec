#include <gmock/gmock.h>

#include <vector>

#include "rule_extractor_helper.h"

using namespace std;

namespace extractor {

typedef unordered_map<int, int> Indexes;

class MockRuleExtractorHelper : public RuleExtractorHelper {
 public:
  MOCK_CONST_METHOD5(GetLinksSpans, void(vector<int>&, vector<int>&,
      vector<int>&, vector<int>&, int));
  MOCK_CONST_METHOD4(CheckAlignedTerminals, bool(const vector<int>&,
      const vector<int>&, const vector<int>&, int));
  MOCK_CONST_METHOD4(CheckTightPhrases, bool(const vector<int>&,
      const vector<int>&, const vector<int>&, int));
  MOCK_CONST_METHOD1(GetGapOrder, vector<int>(const vector<pair<int, int> >&));
  MOCK_CONST_METHOD4(GetSourceIndexes, Indexes(const vector<int>&,
      const vector<int>&, int, int));

  // We need to implement these methods, because Google Mock doesn't support
  // methods with more than 10 arguments.
  bool FindFixPoint(
      int, int, const vector<int>&, const vector<int>&, int& target_phrase_low,
      int& target_phrase_high, const vector<int>&, const vector<int>&,
      int& source_back_low, int& source_back_high, int, int, int, int, bool,
      bool, bool) const {
    target_phrase_low = this->target_phrase_low;
    target_phrase_high = this->target_phrase_high;
    source_back_low = this->source_back_low;
    source_back_high = this->source_back_high;
    return find_fix_point;
  }

  bool GetGaps(vector<pair<int, int> >& source_gaps,
               vector<pair<int, int> >& target_gaps,
               const vector<int>&, const vector<int>&, const vector<int>&,
               const vector<int>&, const vector<int>&, const vector<int>&,
               int, int, int, int, int, int, int& num_symbols,
               bool& met_constraints) const {
    source_gaps = this->source_gaps;
    target_gaps = this->target_gaps;
    num_symbols = this->num_symbols;
    met_constraints = this->met_constraints;
    return get_gaps;
  }

  void SetUp(
      int target_phrase_low, int target_phrase_high, int source_back_low,
      int source_back_high, bool find_fix_point,
      vector<pair<int, int> > source_gaps, vector<pair<int, int> > target_gaps,
      int num_symbols, bool met_constraints, bool get_gaps) {
    this->target_phrase_low = target_phrase_low;
    this->target_phrase_high = target_phrase_high;
    this->source_back_low = source_back_low;
    this->source_back_high = source_back_high;
    this->find_fix_point = find_fix_point;
    this->source_gaps = source_gaps;
    this->target_gaps = target_gaps;
    this->num_symbols = num_symbols;
    this->met_constraints = met_constraints;
    this->get_gaps = get_gaps;
  }

 private:
  int target_phrase_low;
  int target_phrase_high;
  int source_back_low;
  int source_back_high;
  bool find_fix_point;
  vector<pair<int, int> > source_gaps;
  vector<pair<int, int> > target_gaps;
  int num_symbols;
  bool met_constraints;
  bool get_gaps;
};

} // namespace extractor
