#include "grammar.h"

#include "rule.h"

Grammar::Grammar(const vector<Rule>& rules,
                 const vector<string>& feature_names) :
  rules(rules), feature_names(feature_names) {}

ostream& operator<<(ostream& os, const Grammar& grammar) {
  for (Rule rule: grammar.rules) {
    os << "[X] ||| " << rule.source_phrase << " ||| "
                     << rule.target_phrase << " |||";
    for (size_t i = 0; i < rule.scores.size(); ++i) {
      os << " " << grammar.feature_names[i] << "=" << rule.scores[i];
    }
    os << " |||";
    for (auto link: rule.alignment) {
      os << " " << link.first << "-" << link.second;
    }
    os << endl;
  }

  return os;
}
