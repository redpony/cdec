#ifndef SEARCH_WORD__
#define SEARCH_WORD__

#include "lm/word_index.hh"

#include <boost/functional/hash.hpp>

#include <string>
#include <utility>

namespace search {

class Word {
  public:
    // Construct a non-terminal.
    Word() : entry_(NULL) {}

    explicit Word(const std::pair<const std::string, lm::WordIndex> &entry) {
      entry_ = &entry;
    }

    // Returns true for two non-terminals even if their labels are different (since we don't care about labels).
    bool operator==(const Word &other) const {
      return entry_ == other.entry_;
    }

    bool Terminal() const { return entry_ != NULL; }

    const std::string &String() const { return entry_->first; }

    lm::WordIndex Index() const { return entry_->second; }

  protected:
    friend size_t hash_value(const Word &word);

    const std::pair<const std::string, lm::WordIndex> *Entry() const { return entry_; }

  private:
    const std::pair<const std::string, lm::WordIndex> *entry_;
};

inline size_t hash_value(const Word &word) {
  return boost::hash_value(word.Entry());
}

} // namespace search
#endif // SEARCH_WORD__
