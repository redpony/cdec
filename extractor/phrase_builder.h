#ifndef _PHRASE_BUILDER_H_
#define _PHRASE_BUILDER_H_

#include <memory>
#include <vector>

using namespace std;

namespace extractor {

class Phrase;
class Vocabulary;

/**
 * Component for constructing phrases.
 */
class PhraseBuilder {
 public:
  PhraseBuilder(shared_ptr<Vocabulary> vocabulary);

  // Constructs a phrase starting from an array of symbols.
  Phrase Build(const vector<int>& symbols);

  // Extends a phrase with a leading and/or trailing nonterminal.
  Phrase Extend(const Phrase& phrase, bool start_x, bool end_x);

 private:
  shared_ptr<Vocabulary> vocabulary;
};

} // namespace extractor

#endif
