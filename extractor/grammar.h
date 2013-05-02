#ifndef _GRAMMAR_H_
#define _GRAMMAR_H_

#include <iostream>
#include <string>
#include <vector>

using namespace std;

namespace extractor {

class Rule;

/**
 * Grammar class wrapping the set of rules to be extracted.
 */
class Grammar {
 public:
  Grammar(const vector<Rule>& rules, const vector<string>& feature_names);

  vector<Rule> GetRules() const;

  vector<string> GetFeatureNames() const;

  friend ostream& operator<<(ostream& os, const Grammar& grammar);

 private:
  vector<Rule> rules;
  vector<string> feature_names;
};

} // namespace extractor

#endif
