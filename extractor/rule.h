#ifndef _RULE_H_
#define _RULE_H_

#include <vector>

#include "phrase.h"

using namespace std;

namespace extractor {

/**
 * Structure containing the data for a SCFG rule.
 */
struct Rule {
  Rule(const Phrase& source_phrase, const Phrase& target_phrase,
       const vector<double>& scores, const vector<pair<int, int>>& alignment);

  Phrase source_phrase;
  Phrase target_phrase;
  vector<double> scores;
  vector<pair<int, int>> alignment;
};

} // namespace extractor

#endif
