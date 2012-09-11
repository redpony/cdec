#include "search/rule.hh"

#include "search/context.hh"
#include "search/final.hh"

#include <ostream>

#include <math.h>

namespace search {

template <class Model> void Rule::FinishedAdding(const Context<Model> &context, Score additive, bool prepend_bos) {
  additive_ = additive;
  Score lm_score = 0.0;
  lexical_.clear();
  const lm::WordIndex oov = context.LanguageModel().GetVocabulary().NotFound();

  for (std::vector<Word>::const_iterator word = items_.begin(); ; ++word) {
    lexical_.resize(lexical_.size() + 1);
    lm::ngram::RuleScore<Model> scorer(context.LanguageModel(), lexical_.back());
    // TODO: optimize
    if (prepend_bos && (word == items_.begin())) {
      scorer.BeginSentence();
    }
    for (; ; ++word) {
      if (word == items_.end()) {
        lm_score += scorer.Finish();
        bound_ = additive_ + context.GetWeights().LM() * lm_score;
        assert(lexical_.size() == arity_ + 1);
        return;
      }
      if (!word->Terminal()) break;
      if (word->Index() == oov) additive_ += context.GetWeights().OOV();
      scorer.Terminal(word->Index());
    }
    lm_score += scorer.Finish();
  }
}

template void Rule::FinishedAdding(const Context<lm::ngram::RestProbingModel> &context, Score additive, bool prepend_bos);
template void Rule::FinishedAdding(const Context<lm::ngram::ProbingModel> &context, Score additive, bool prepend_bos);

std::ostream &operator<<(std::ostream &o, const Rule &rule) {
  const Rule::ItemsRet &items = rule.Items();
  for (Rule::ItemsRet::const_iterator i = items.begin(); i != items.end(); ++i) {
    if (i->Terminal()) {
      o << i->String() << ' ';
    } else {
      o << "[] ";
    }
  }
  return o;
}

} // namespace search
