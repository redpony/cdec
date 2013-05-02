#include "max_lex_target_given_source.h"

#include <cmath>

#include "data_array.h"
#include "translation_table.h"

namespace extractor {
namespace features {

MaxLexTargetGivenSource::MaxLexTargetGivenSource(
    shared_ptr<TranslationTable> table) :
    table(table) {}

double MaxLexTargetGivenSource::Score(const FeatureContext& context) const {
  vector<string> source_words = context.source_phrase.GetWords();
  source_words.push_back(DataArray::NULL_WORD_STR);
  vector<string> target_words = context.target_phrase.GetWords();

  double score = 0;
  for (string target_word: target_words) {
    double max_score = 0;
    for (string source_word: source_words) {
      max_score = max(max_score,
          table->GetTargetGivenSourceScore(source_word, target_word));
    }
    score += max_score > 0 ? -log10(max_score) : MAX_SCORE;
  }
  return score;
}

string MaxLexTargetGivenSource::GetName() const {
  return "MaxLexEgivenF";
}

} // namespace features
} // namespace extractor
