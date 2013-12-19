#ifndef _RULE_LEXER_H_
#define _RULE_LEXER_H_

#include <iostream>
#include <string>

#include "trule.h"

struct RuleLexer {
  typedef void (*RuleCallback)(const TRulePtr& new_rule, const unsigned int ctf_level, const TRulePtr& coarse_rule, void* extra);
  static void ReadRules(std::istream* in, RuleCallback func, const std::string& fname, void* extra);
};

#endif
