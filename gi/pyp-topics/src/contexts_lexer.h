#ifndef _CONTEXTS_LEXER_H_
#define _CONTEXTS_LEXER_H_ 

#include <iostream>
#include <vector>
#include <string>

#include "dict.h" 

struct ContextsLexer {
  typedef std::vector<std::string> Context;
  struct PhraseContextsType {
    std::string          phrase;
    std::vector<Context> contexts;
    std::vector< std::pair<int,int> >     counts;
  };

  typedef void (*ContextsCallback)(const PhraseContextsType& new_contexts, void* extra);
  static void ReadContexts(std::istream* in, ContextsCallback func, void* extra);
};

#endif
