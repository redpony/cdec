#ifndef _TARGET_PHRASE_EXTRACTOR_H_
#define _TARGET_PHRASE_EXTRACTOR_H_

#include <memory>
#include <unordered_map>
#include <vector>

using namespace std;

namespace extractor {

typedef vector<pair<int, int> > PhraseAlignment;

class Alignment;
class DataArray;
class Phrase;
class PhraseBuilder;
class RuleExtractorHelper;
class Vocabulary;

class TargetPhraseExtractor {
 public:
  TargetPhraseExtractor(shared_ptr<DataArray> target_data_array,
                        shared_ptr<Alignment> alignment,
                        shared_ptr<PhraseBuilder> phrase_builder,
                        shared_ptr<RuleExtractorHelper> helper,
                        shared_ptr<Vocabulary> vocabulary,
                        int max_rule_span,
                        bool require_tight_phrases);

  virtual ~TargetPhraseExtractor();

  // Finds all the target phrases that can extracted from a span in the
  // target sentence (matching the given set of target phrase gaps).
  virtual vector<pair<Phrase, PhraseAlignment> > ExtractPhrases(
      const vector<pair<int, int> >& target_gaps, const vector<int>& target_low,
      int target_phrase_low, int target_phrase_high,
      const unordered_map<int, int>& source_indexes, int sentence_id) const;

 protected:
  TargetPhraseExtractor();

 private:
  // Computes the cartesian product over the sets of possible target phrase
  // chunks.
  void GeneratePhrases(
      vector<pair<Phrase, PhraseAlignment> >& target_phrases,
      const vector<pair<int, int> >& ranges, int index,
      vector<int>& subpatterns, const vector<int>& target_gap_order,
      int target_phrase_low, int target_phrase_high,
      const unordered_map<int, int>& source_indexes, int sentence_id) const;

  shared_ptr<DataArray> target_data_array;
  shared_ptr<Alignment> alignment;
  shared_ptr<PhraseBuilder> phrase_builder;
  shared_ptr<RuleExtractorHelper> helper;
  shared_ptr<Vocabulary> vocabulary;
  int max_rule_span;
  bool require_tight_phrases;
};

} // namespace extractor

#endif
