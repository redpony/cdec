#ifndef SEARCH_RULE__
#define SEARCH_RULE__

#include "lm/left.hh"
#include "search/arity.hh"
#include "search/types.hh"
#include "search/word.hh"

#include <boost/array.hpp>

#include <iosfwd>
#include <vector>

namespace search {

template <class Model> class Context;

class Rule {
  public:
    Rule() : arity_(0) {}

    void AppendTerminal(Word w) { items_.push_back(w); }

    void AppendNonTerminal() {
      items_.resize(items_.size() + 1);
      ++arity_;
    }

    template <class Model> void FinishedAdding(const Context<Model> &context, Score additive, bool prepend_bos);

    Score Bound() const { return bound_; }

    Score Additive() const { return additive_; }

    unsigned int Arity() const { return arity_; }

    const lm::ngram::ChartState &Lexical(unsigned int index) const {
      return lexical_[index];
    }

    // For printing.  
    typedef const std::vector<Word> ItemsRet;
    ItemsRet &Items() const { return items_; }

  private:
    Score bound_, additive_;

    unsigned int arity_;

    // TODO: pool?
    std::vector<Word> items_;

    std::vector<lm::ngram::ChartState> lexical_;
};

std::ostream &operator<<(std::ostream &o, const Rule &rule);

} // namespace search

#endif // SEARCH_RULE__
