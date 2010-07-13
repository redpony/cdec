%{
#include "rule_lexer.h"

#include <string>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cassert>
#include "tdict.h"
#include "fdict.h"
#include "trule.h"
#include "striped_grammar.h"

int lex_line = 0;
std::istream* sglex_stream = NULL;
StripedGrammarLexer::GrammarCallback grammar_callback = NULL;
void* grammar_callback_extra = NULL;

#undef YY_INPUT
#define YY_INPUT(buf, result, max_size) (result = sglex_stream->read(buf, max_size).gcount())

#define YY_SKIP_YYWRAP 1
int num_rules = 0;
int yywrap() { return 1; }
bool fl = true;
#define MAX_TOKEN_SIZE 255
std::string sglex_tmp_token(MAX_TOKEN_SIZE, '\0');

#define MAX_RULE_SIZE 48
WordID sglex_src_rhs[MAX_RULE_SIZE];
WordID sglex_trg_rhs[MAX_RULE_SIZE];
int sglex_src_rhs_size;
int sglex_trg_rhs_size;
WordID sglex_lhs;
int sglex_src_arity;
int sglex_trg_arity;

#define MAX_FEATS 100
int sglex_feat_ids[MAX_FEATS];
double sglex_feat_vals[MAX_FEATS];
int sglex_num_feats;

#define MAX_ARITY 20
int sglex_nt_sanity[MAX_ARITY];
int sglex_src_nts[MAX_ARITY];
float sglex_nt_size_means[MAX_ARITY];
float sglex_nt_size_vars[MAX_ARITY];

std::vector<WordID> cur_src_rhs;
std::vector<WordID> cur_trg_rhs;
ID2RuleStatistics cur_options;
RuleStatistics* cur_stats = NULL;
int sglex_cur_fid = 0;

static void sanity_check_trg_index(int index) {
  if (index > sglex_src_arity) {
    std::cerr << "Target index " << index << " exceeds source arity " << sglex_src_arity << std::endl;
    abort();
  }
  int& flag = sglex_nt_sanity[index - 1];
  if (flag) {
    std::cerr << "Target index " << index << " used multiple times!" << std::endl;
    abort();
  }
  flag = 1;
}

static void sglex_reset() {
  sglex_src_arity = 0;
  sglex_trg_arity = 0;
  sglex_num_feats = 0;
  sglex_src_rhs_size = 0;
  sglex_trg_rhs_size = 0;
}

%}

REAL [\-+]?[0-9]+(\.[0-9]*([eE][-+]*[0-9]+)?)?|inf|[\-+]inf
NT [\-#$A-Z_:=.",\\][\-#$".A-Z+/=_0-9!:@\\]*
ALIGN [0-9]+-[0-9]+

%x LHS_END SRC TRG FEATS FEATVAL ALIGNS
%%

<INITIAL>[ ]	;

<INITIAL>\[{NT}\]   {
		sglex_tmp_token.assign(yytext + 1, yyleng - 2);
		sglex_lhs = -TD::Convert(sglex_tmp_token);
		// std::cerr << sglex_tmp_token << "\n";
  		BEGIN(LHS_END);
		}

<SRC>\[{NT}\]   {
		sglex_tmp_token.assign(yytext + 1, yyleng - 2);
		sglex_src_nts[sglex_src_arity] = sglex_src_rhs[sglex_src_rhs_size] = -TD::Convert(sglex_tmp_token);
		++sglex_src_arity;
		++sglex_src_rhs_size;
		}

<LHS_END>[ ] { ; }
<LHS_END>\|\|\|	{
		sglex_reset();
		BEGIN(SRC);
		}
<INITIAL,LHS_END>.	{
		std::cerr << "Line " << lex_line << ": unexpected input in LHS: " << yytext << std::endl;
		exit(1);
		}


<SRC>\[{NT},[1-9][0-9]?\]   {
		int index = yytext[yyleng - 2] - '0';
		if (yytext[yyleng - 3] == ',') {
		  sglex_tmp_token.assign(yytext + 1, yyleng - 4);
		} else {
		  sglex_tmp_token.assign(yytext + 1, yyleng - 5);
		  index += 10 * (yytext[yyleng - 3] - '0');
		}
		if ((sglex_src_arity+1) != index) {
			std::cerr << "Src indices must go in order: expected " << sglex_src_arity << " but got " << index << std::endl;
			abort();
		}
		sglex_src_nts[sglex_src_arity] = sglex_src_rhs[sglex_src_rhs_size] = -TD::Convert(sglex_tmp_token);
		++sglex_src_rhs_size;
		++sglex_src_arity;
		}

<SRC>[^ \t]+	{ 
		sglex_tmp_token.assign(yytext, yyleng);
		sglex_src_rhs[sglex_src_rhs_size] = TD::Convert(sglex_tmp_token);
		++sglex_src_rhs_size;
		}
<SRC>[ ]	{ ; }
<SRC>\t		{
		//std::cerr << "LHS=" << TD::Convert(-sglex_lhs) << " ";
		//std::cerr << "  src_size: " << sglex_src_rhs_size << std::endl;
		//std::cerr << "  src_arity: " << sglex_src_arity << std::endl;
		memset(sglex_nt_sanity, 0, sglex_src_arity * sizeof(int));
		cur_options.clear();
		sglex_trg_rhs_size = 0;
		BEGIN(TRG);
		}

<TRG>\[[1-9][0-9]?\]   {
		int index = yytext[yyleng - 2] - '0';
		if (yyleng == 4) {
		  index += 10 * (yytext[yyleng - 3] - '0');
		}
		++sglex_trg_arity;
		sanity_check_trg_index(index);
		sglex_trg_rhs[sglex_trg_rhs_size] = 1 - index;
		++sglex_trg_rhs_size;
}

<TRG>\|\|\|	{
		assert(sglex_trg_rhs_size > 0);
		cur_trg_rhs.resize(sglex_trg_rhs_size);
		for (int i = 0; i < sglex_trg_rhs_size; ++i)
			cur_trg_rhs[i] = sglex_trg_rhs[i];
		cur_stats = &cur_options[cur_trg_rhs];
		BEGIN(FEATS);
		}

<TRG>[^ ]+	{
		sglex_tmp_token.assign(yytext, yyleng);
		sglex_trg_rhs[sglex_trg_rhs_size] = TD::Convert(sglex_tmp_token);
		
		++sglex_trg_rhs_size;
		}
<TRG>[ ]+	{ ; }

<FEATS>\n	{
		assert(sglex_lhs < 0);
		assert(sglex_src_rhs_size > 0);
		cur_src_rhs.resize(sglex_src_rhs_size);
		for (int i = 0; i < sglex_src_rhs_size; ++i)
			cur_src_rhs[i] = sglex_src_rhs[i];
		grammar_callback(sglex_lhs, cur_src_rhs, cur_options, grammar_callback_extra);
		cur_options.clear();
		BEGIN(INITIAL);
		}
<FEATS>[ ]+	{ ; }
<FEATS>\|\|\|	{
		memset(sglex_nt_sanity, 0, sglex_src_arity * sizeof(int));
		sglex_trg_rhs_size = 0;
		BEGIN(TRG);
		}
<FEATS>[A-Z][A-Z_0-9]*=	{
		// std::cerr << "FV: " << yytext << std::endl;
		sglex_tmp_token.assign(yytext, yyleng - 1);
		sglex_cur_fid = FD::Convert(sglex_tmp_token);
		static const int Afid = FD::Convert("A");
		if (sglex_cur_fid == Afid) {
			BEGIN(ALIGNS);
		} else {
			BEGIN(FEATVAL);
		}
		}
<FEATVAL>{REAL}	{
		// std::cerr << "Feature val input: " << yytext << std::endl;
		cur_stats->counts.set_value(sglex_cur_fid, strtod(yytext, NULL));
		BEGIN(FEATS);
		}
<FEATVAL>.	{
		std::cerr << "Feature val unexpected input: " << yytext << std::endl;
		exit(1);
		}
<FEATS>.	{
		std::cerr << "Features unexpected input: " << yytext << std::endl;
		exit(1);
		}
<ALIGNS>{ALIGN}(,{ALIGN})*	{
		assert(cur_stats->aligns.empty());
		int i = 0;
		while(i < yyleng) {
			short a = 0;
			short b = 0;
			while (yytext[i] != '-') { a *= 10; a += yytext[i] - '0'; ++i; }
			++i;
			while (yytext[i] != ',' && i < yyleng) { b *= 10; b += yytext[i] - '0'; ++i; }
			++i;
			cur_stats->aligns.push_back(std::make_pair(a,b));
		}
		BEGIN(FEATS);
		}
<ALIGNS>.	{
		std::cerr << "Aligns unexpected input: " << yytext << std::endl;
		exit(1);
		}
%%

#include "filelib.h"

void StripedGrammarLexer::ReadStripedGrammar(std::istream* in, GrammarCallback func, void* extra) {
  lex_line = 1;
  sglex_stream = in;
  grammar_callback_extra = extra;
  grammar_callback = func;
  yylex();
}

