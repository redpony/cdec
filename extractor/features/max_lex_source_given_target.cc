#include "max_lex_source_given_target.h"

#include <cmath>

#include "../translation_table.h"

MaxLexSourceGivenTarget::MaxLexSourceGivenTarget(
    shared_ptr<TranslationTable> table) :
    table(table) {}

double MaxLexSourceGivenTarget::Score(const FeatureContext& context) const {
  vector<string> source_words = context.source_phrase.GetWords();
  // TODO(pauldb): Add NULL to target_words, after fixing translation table.
  vector<string> target_words = context.target_phrase.GetWords();

  double score = 0;
  for (string source_word: source_words) {
    double max_score = 0;
    for (string target_word: target_words) {
      max_score = max(max_score,
          table->GetSourceGivenTargetScore(source_word, target_word));
    }
    score += max_score > 0 ? -log10(max_score) : MAX_SCORE;
  }
  return score;
}

string MaxLexSourceGivenTarget::GetName() const {
  return "MaxLexFGivenE";
}
