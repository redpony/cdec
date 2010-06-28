%{
#include "contexts_lexer.h"

#include <string>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cassert>

int lex_line = 0;
std::istream* contextslex_stream = NULL;
ContextsLexer::ContextsCallback contexts_callback = NULL;
void* contexts_callback_extra = NULL;

#undef YY_INPUT
#define YY_INPUT(buf, result, max_size) (result = contextslex_stream->read(buf, max_size).gcount())

#define YY_SKIP_YYWRAP 1
int num_phrases = 0;
int yywrap() { return 1; }

#define MAX_TOKEN_SIZE 255
std::string contextslex_tmp_token(MAX_TOKEN_SIZE, '\0');
ContextsLexer::PhraseContextsType current_contexts;

#define MAX_CONTEXT_SIZE 255
//std::string tmp_context[MAX_CONTEXT_SIZE];
ContextsLexer::Context tmp_context;


void contextslex_reset() {
  current_contexts.phrase.clear();
  current_contexts.contexts.clear();
  current_contexts.counts.clear();
  tmp_context.clear();
}

%}

INT [\-+]?[0-9]+|inf|[\-+]inf

%x CONTEXT COUNT COUNT_END
%%

<INITIAL>[^\t]+	{ 
    contextslex_reset();
    current_contexts.phrase.assign(yytext, yyleng);
    BEGIN(CONTEXT);
  }
<INITIAL>\t	{ 
    ; 
  }

<INITIAL,CONTEXT,COUNT>\n	{
    std::cerr << "ERROR: contexts_lexer.l: unexpected newline while trying to read phrase|context|count." << std::endl;
    abort();
  }

<CONTEXT>\|\|\|	{
    current_contexts.contexts.push_back(tmp_context);
    tmp_context.clear();
		BEGIN(COUNT);
	}
<CONTEXT>[^ \t]+	{ 
		contextslex_tmp_token.assign(yytext, yyleng);
    tmp_context.push_back(contextslex_tmp_token);
  }
<CONTEXT>[ \t]+	{ ; }

<COUNT>[ \t]+	{ ; }
<COUNT>C={INT} { 
		current_contexts.counts.push_back(atoi(yytext+2));
    BEGIN(COUNT_END);
  }
<COUNT>.	{ 
    std::cerr << "ERROR: contexts_lexer.l: unexpected content while reading count." << std::endl;
    abort();
  }

<COUNT_END>[ \t]+  { ; }
<COUNT_END>\|\|\|	{
		BEGIN(CONTEXT);
  }
<COUNT_END>\n {
    //std::cerr << "READ:" << current_contexts.phrase << " with " << current_contexts.contexts.size() 
    //  << " contexts, and " << current_contexts.counts.size() << " counts." << std::endl;
		contexts_callback(current_contexts, contexts_callback_extra);
    current_contexts.phrase.clear();
    current_contexts.contexts.clear();
    current_contexts.counts.clear();
		BEGIN(INITIAL);
  }
<COUNT_END>.  { 
		contextslex_tmp_token.assign(yytext, yyleng);
    std::cerr << "ERROR: contexts_lexer.l: unexpected content while looking for ||| closing count." << std::endl;
    abort();
  }

%%

#include "../../../decoder/filelib.h" 

void ContextsLexer::ReadContexts(std::istream* in, ContextsLexer::ContextsCallback func, void* extra) {
  lex_line = 1;
  contextslex_stream = in;
  contexts_callback_extra = extra,
  contexts_callback = func;
  yylex();
}

