#ifndef _RULE_LEXER_H_
#define _RULE_LEXER_H_

#include <iostream>
#include <string>

#include "sparse_vector.h"

struct JSONFeatureMapLexer {
  typedef void (*FeatureMapCallback)(const std::string& id, const SparseVector<float>& fmap, void* extra);
  static void ReadRules(std::istream* in, FeatureMapCallback func, void* extra);
};

#endif

