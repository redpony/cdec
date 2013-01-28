#ifndef _PHRASE_BUILDER_H_
#define _PHRASE_BUILDER_H_

#include <memory>
#include <vector>

using namespace std;

class Phrase;
class Vocabulary;

class PhraseBuilder {
 public:
  PhraseBuilder(shared_ptr<Vocabulary> vocabulary);

  Phrase Build(const vector<int>& symbols);

 private:
  shared_ptr<Vocabulary> vocabulary;
};

#endif
