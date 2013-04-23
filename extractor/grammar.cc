#include "grammar.h"

#include <iomanip>

#include "rule.h"

using namespace std;

namespace extractor {

Grammar::Grammar(const vector<Rule>& rules,
                 const vector<string>& feature_names) :
  rules(rules), feature_names(feature_names) {}

vector<Rule> Grammar::GetRules() const {
  return rules;
}

vector<string> Grammar::GetFeatureNames() const {
  return feature_names;
}

ostream& operator<<(ostream& os, const Grammar& grammar) {
  vector<Rule> rules = grammar.GetRules();
  vector<string> feature_names = grammar.GetFeatureNames();
  os << setprecision(12);
  for (Rule rule: rules) {
    os << "[X] ||| " << rule.source_phrase << " ||| "
                     << rule.target_phrase << " |||";
    for (size_t i = 0; i < rule.scores.size(); ++i) {
      os << " " << feature_names[i] << "=" << rule.scores[i];
    }
    os << " |||";
    for (auto link: rule.alignment) {
      os << " " << link.first << "-" << link.second;
    }
    os << '\n';
  }

  return os;
}

} // namespace extractor
