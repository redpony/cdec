#include "rule.h"

namespace extractor {

Rule::Rule(const Phrase& source_phrase,
           const Phrase& target_phrase,
           const vector<double>& scores,
           const vector<pair<int, int> >& alignment) :
  source_phrase(source_phrase),
  target_phrase(target_phrase),
  scores(scores),
  alignment(alignment) {}

} // namespace extractor
