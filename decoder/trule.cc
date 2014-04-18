#include "trule.h"

#include <sstream>

#include "stringlib.h"
#include "tdict.h"
#include "rule_lexer.h"

using namespace std;

ostream &operator<<(ostream &o,TRule const& r) {
  return o<<r.AsString(true);
}

bool TRule::IsGoal() const {
  static const int kGOAL(TD::Convert("Goal") * -1); // this will happen once, and after static init of trule.cc static dict.
  return GetLHS() == kGOAL;
}

TRule* TRule::CreateRuleSynchronous(const string& rule) {
  TRule* res = new TRule;
  if (res->ReadFromString(rule)) return res;
  cerr << "[ERROR] Failed to creating rule from: " << rule << endl;
  delete res;
  return NULL;
}

TRule* TRule::CreateRulePhrasetable(const string& rule) {
  TRule* res = new TRule("[X] ||| " + rule);
  if (res->Arity() != 0) {
    cerr << "Phrasetable rules should have arity 0:\n  " << rule << endl;
    delete res;
    return NULL;
  }
  return res;
}

TRule* TRule::CreateRuleMonolingual(const string& rule) {
  return new TRule(rule, true);
}

namespace {
// callback for single rule lexer
int n_assigned=0;
  void assign_trule(const TRulePtr& new_rule, const unsigned int ctf_level, const TRulePtr& coarse_rule, void* extra) {
    (void) ctf_level;
    (void) coarse_rule;
    *static_cast<TRule*>(extra) = *new_rule;
    ++n_assigned;
  }
}

bool TRule::ReadFromString(const string& line, bool mono) {
  n_assigned = 0;
  //cerr << "LINE: " << line << "  -- mono=" << mono << endl;
  RuleLexer::ReadRule(line + '\n', assign_trule, mono, this);
  if (n_assigned > 1)
    cerr<<"\nWARNING: more than one rule parsed from multi-line string; kept last: "<<line<<".\n";
  if (mono) {
    e_ = f_;
    int ntc = 0;
    for (auto& i : e_)
      if (i < 0) i = -ntc++;
  }
  return n_assigned;
}

void TRule::ComputeArity() {
  int min = 1;
  for (vector<WordID>::const_iterator i = e_.begin(); i != e_.end(); ++i)
    if (*i < min) min = *i;
  arity_ = 1 - min;
}

string TRule::AsString(bool verbose) const {
  ostringstream os;
  int idx = 0;
  if (lhs_) {
    os << '[' << TD::Convert(lhs_ * -1) << "] |||";
  } else { os << "NOLHS |||"; }
  for (unsigned i = 0; i < f_.size(); ++i) {
    const WordID& w = f_[i];
    if (w < 0) {
      int wi = w * -1;
      ++idx;
      os << " [" << TD::Convert(wi) << ']';
    } else {
      os << ' ' << TD::Convert(w);
    }
  }
  os << " ||| ";
  for (unsigned i =0; i<e_.size(); ++i) {
    if (i) os << ' ';
    const WordID& w = e_[i];
    if (w < 1)
      os << '[' << (1-w) << ']';
    else
      os << TD::Convert(w);
  }
  if (!scores_.empty() && verbose) {
    os << " ||| " << scores_;
    if (!a_.empty()) {
      os << " |||";
      for (unsigned i = 0; i < a_.size(); ++i)
        os << ' ' << a_[i];
    }
  }
  return os.str();
}
