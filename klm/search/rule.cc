#include "search/rule.hh"

#include "search/context.hh"
#include "search/final.hh"

#include <ostream>

#include <math.h>

namespace search {

template <class Model> void Rule::Init(const Context<Model> &context, Score additive, const std::vector<lm::WordIndex> &words, bool prepend_bos) {
  additive_ = additive;
  Score lm_score = 0.0;
  lexical_.clear();
  const lm::WordIndex oov = context.LanguageModel().GetVocabulary().NotFound();

  for (std::vector<lm::WordIndex>::const_iterator word = words.begin(); ; ++word) {
    lexical_.resize(lexical_.size() + 1);
    lm::ngram::RuleScore<Model> scorer(context.LanguageModel(), lexical_.back());
    // TODO: optimize
    if (prepend_bos && (word == words.begin())) {
      scorer.BeginSentence();
    }
    for (; ; ++word) {
      if (word == words.end()) {
        lm_score += scorer.Finish();
        bound_ = additive_ + context.GetWeights().LM() * lm_score;
        arity_ = lexical_.size() - 1; 
        return;
      }
      if (*word == kNonTerminal) break;
      if (*word == oov) additive_ += context.GetWeights().OOV();
      scorer.Terminal(*word);
    }
    lm_score += scorer.Finish();
  }
}

template void Rule::Init(const Context<lm::ngram::RestProbingModel> &context, Score additive, const std::vector<lm::WordIndex> &words, bool prepend_bos);
template void Rule::Init(const Context<lm::ngram::ProbingModel> &context, Score additive, const std::vector<lm::WordIndex> &words, bool prepend_bos);

} // namespace search
