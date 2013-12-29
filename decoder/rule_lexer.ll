%option nounput
%{
#include "rule_lexer.h"

#include <string>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cassert>
#include <stack>
#include "tdict.h"
#include "fdict.h"
#include "trule.h"
#include "verbose.h"

int lex_line = 0;
std::istream* scfglex_stream = NULL;
RuleLexer::RuleCallback rule_callback = NULL;
void* rule_callback_extra = NULL;
std::vector<int> scfglex_phrase_fnames;
std::string scfglex_fname;

#undef YY_INPUT
#define YY_INPUT(buf, result, max_size) (result = scfglex_stream->read(buf, max_size).gcount())

#define YY_SKIP_YYWRAP 1
int num_rules = 0;
int yywrap() { return 1; }
bool fl = true;
#define MAX_TOKEN_SIZE 255
std::string scfglex_tmp_token(MAX_TOKEN_SIZE, '\0');

#define MAX_RULE_SIZE 200
WordID scfglex_src_rhs[MAX_RULE_SIZE];
WordID scfglex_trg_rhs[MAX_RULE_SIZE];
int scfglex_src_rhs_size;
int scfglex_trg_rhs_size;
WordID scfglex_lhs;
int scfglex_src_arity;
int scfglex_trg_arity;

#define MAX_FEATS 10000
int scfglex_feat_ids[MAX_FEATS];
double scfglex_feat_vals[MAX_FEATS];
int scfglex_num_feats;

#define MAX_ARITY 1000
int scfglex_nt_sanity[MAX_ARITY];
int scfglex_src_nts[MAX_ARITY];
// float scfglex_nt_size_means[MAX_ARITY];
// float scfglex_nt_size_vars[MAX_ARITY];
std::stack<TRulePtr> ctf_rule_stack;
unsigned int ctf_level = 0;

#define MAX_ALS 2000
AlignmentPoint scfglex_als[MAX_ALS];
int scfglex_num_als;

void sanity_check_trg_symbol(WordID nt, int index) {
  if (scfglex_src_nts[index-1] != nt) {
    std::cerr << "Target symbol with index " << index << " is of type " << TD::Convert(nt*-1)
              << " but corresponding source is of type "
              << TD::Convert(scfglex_src_nts[index-1] * -1) << std::endl;
    abort();
  }
}

void sanity_check_trg_index(int index) {
  if (index > scfglex_src_arity) {
    std::cerr << "Target index " << index << " exceeds source arity " << scfglex_src_arity << std::endl;
    abort();
  }
  int& flag = scfglex_nt_sanity[index - 1];
  if (flag) {
    std::cerr << "Target index " << index << " used multiple times!" << std::endl;
    abort();
  }
  flag = 1;
}

void scfglex_reset() {
  scfglex_src_arity = 0;
  scfglex_trg_arity = 0;
  scfglex_num_feats = 0;
  scfglex_src_rhs_size = 0;
  scfglex_trg_rhs_size = 0;
  scfglex_num_als = 0;
}

void check_and_update_ctf_stack(const TRulePtr& rp) {
  if (ctf_level > ctf_rule_stack.size()){
    std::cerr << "Found rule at projection level " << ctf_level << " but previous rule was at level "
      << ctf_rule_stack.size()-1 << " (cannot exceed previous level by more than one; line " << lex_line << ")" << std::endl;
    abort();
  }
  while (ctf_rule_stack.size() > ctf_level)
    ctf_rule_stack.pop();
  // ensure that rule has the same signature as parent (coarse) rule.  Rules may *only*
  // differ by the rhs nonterminals, not terminals or permutation of nonterminals.
  if (ctf_rule_stack.size() > 0) {
    TRulePtr& coarse_rp = ctf_rule_stack.top();
    if (rp->f_.size() != coarse_rp->f_.size() || rp->e_ != coarse_rp->e_) {
      std::cerr << "Rule " << (rp->AsString()) << " is not a projection of " <<
        (coarse_rp->AsString()) << std::endl;
      abort();
    }
    for (int i=0; i<rp->f_.size(); ++i) {
      if (((rp->f_[i]<0) != (coarse_rp->f_[i]<0)) ||
          ((rp->f_[i]>0) && (rp->f_[i] != coarse_rp->f_[i]))) {
        std::cerr << "Rule " << (rp->AsString()) << " is not a projection of " <<
          (coarse_rp->AsString()) << std::endl;
        abort();
      }
    }
  }
}

%}

REAL [\-+]?[0-9]+(\.[0-9]*([eE][-+]*[0-9]+)?)?|inf|[\-+]inf
NT [^\t \[\],]+

%x LHS_END SRC TRG FEATS FEATVAL ALIGNS
%%

<INITIAL>[ \t]	{
  ctf_level++;
  };

<INITIAL>\[{NT}\]   {
		scfglex_tmp_token.assign(yytext + 1, yyleng - 2);
		scfglex_lhs = -TD::Convert(scfglex_tmp_token);
		// std::cerr << scfglex_tmp_token << "\n";
  		BEGIN(LHS_END);
		}

<SRC>\[{NT}\]   {
		scfglex_tmp_token.assign(yytext + 1, yyleng - 2);
		scfglex_src_nts[scfglex_src_arity] = scfglex_src_rhs[scfglex_src_rhs_size] = -TD::Convert(scfglex_tmp_token);
		++scfglex_src_arity;
		++scfglex_src_rhs_size;
		}

<SRC>\[{NT},[1-9][0-9]?\]   {
		int index = yytext[yyleng - 2] - '0';
		if (yytext[yyleng - 3] == ',') {
		  scfglex_tmp_token.assign(yytext + 1, yyleng - 4);
		} else {
		  scfglex_tmp_token.assign(yytext + 1, yyleng - 5);
		  index += 10 * (yytext[yyleng - 3] - '0');
		}
		if ((scfglex_src_arity+1) != index) {
			std::cerr << "Src indices must go in order: expected " << scfglex_src_arity << " but got " << index << std::endl;
			abort();
		}
		scfglex_src_nts[scfglex_src_arity] = scfglex_src_rhs[scfglex_src_rhs_size] = -TD::Convert(scfglex_tmp_token);
		++scfglex_src_rhs_size;
		++scfglex_src_arity;
		}

<TRG>\[{NT},[1-9][0-9]?\]   {
		int index = yytext[yyleng - 2] - '0';
		if (yytext[yyleng - 3] == ',') {
		  scfglex_tmp_token.assign(yytext + 1, yyleng - 4);
		} else {
		  scfglex_tmp_token.assign(yytext + 1, yyleng - 5);
		  index += 10 * (yytext[yyleng - 3] - '0');
		}
		++scfglex_trg_arity;
		// std::cerr << "TRG INDEX: " << index << std::endl;
		sanity_check_trg_symbol(-TD::Convert(scfglex_tmp_token), index);
		sanity_check_trg_index(index);
		scfglex_trg_rhs[scfglex_trg_rhs_size] = 1 - index;
		++scfglex_trg_rhs_size;
}

<TRG>\[[1-9][0-9]?\]   {
		int index = yytext[yyleng - 2] - '0';
		if (yyleng == 4) {
		  index += 10 * (yytext[yyleng - 3] - '0');
		}
		++scfglex_trg_arity;
		sanity_check_trg_index(index);
		scfglex_trg_rhs[scfglex_trg_rhs_size] = 1 - index;
		++scfglex_trg_rhs_size;
}

<LHS_END>[ \t] { ; }
<LHS_END>\|\|\|	{
		scfglex_reset();
		BEGIN(SRC);
		}
<INITIAL,LHS_END>.	{
		std::cerr << "Grammar " << scfglex_fname << " line " << lex_line << ": unexpected input in LHS: " << yytext << std::endl;
		abort();
		}

<SRC>\|\|\|	{
		memset(scfglex_nt_sanity, 0, scfglex_src_arity * sizeof(int));
		BEGIN(TRG);
		}
<SRC>[^ \t]+	{
		scfglex_tmp_token.assign(yytext, yyleng);
		scfglex_src_rhs[scfglex_src_rhs_size] = TD::Convert(scfglex_tmp_token);
		++scfglex_src_rhs_size;
		}
<SRC>[ \t]+	{ ; }

<TRG>\|\|\|	{
		BEGIN(FEATS);
		}
<TRG>[^ \t]+	{
		scfglex_tmp_token.assign(yytext, yyleng);
		scfglex_trg_rhs[scfglex_trg_rhs_size] = TD::Convert(scfglex_tmp_token);
		++scfglex_trg_rhs_size;
		}
<TRG>[ \t]+	{ ; }

<TRG,FEATS,ALIGNS>\n	{
                if (scfglex_src_arity != scfglex_trg_arity) {
                  std::cerr << "Grammar " << scfglex_fname << " line " << lex_line << ": LHS and RHS arity mismatch!\n";
                  abort();
                }
		// const bool ignore_grammar_features = false;
		// if (ignore_grammar_features) scfglex_num_feats = 0;
		TRulePtr rp(new TRule(scfglex_lhs, scfglex_src_rhs, scfglex_src_rhs_size, scfglex_trg_rhs, scfglex_trg_rhs_size, scfglex_feat_ids, scfglex_feat_vals, scfglex_num_feats, scfglex_src_arity, scfglex_als, scfglex_num_als));
    check_and_update_ctf_stack(rp);
    TRulePtr coarse_rp = ((ctf_level == 0) ? TRulePtr() : ctf_rule_stack.top());
		rule_callback(rp, ctf_level, coarse_rp, rule_callback_extra);
    ctf_rule_stack.push(rp);
		// std::cerr << rp->AsString() << std::endl;
		num_rules++;
    lex_line++;
    if (!SILENT) {
      if (num_rules %   50000 == 0) { std::cerr << '.' << std::flush; fl = true; }
      if (num_rules % 2000000 == 0) { std::cerr << " [" << num_rules << "]\n"; fl = false; }
    }
    ctf_level = 0;
		BEGIN(INITIAL);
		}

<FEATS>[ \t;]	{ ; }
<FEATS>[^ \t=;]+=	{
		scfglex_tmp_token.assign(yytext, yyleng - 1);
		const int fid = FD::Convert(scfglex_tmp_token);
		if (fid < 1) {
			std::cerr << "\nUNWEIGHED FEATURE " << scfglex_tmp_token << std::endl;
			abort();
		}
		scfglex_feat_ids[scfglex_num_feats] = fid;
		BEGIN(FEATVAL);
		}
<FEATS>\|\|\|	{
		BEGIN(ALIGNS);
		}
<FEATVAL>{REAL}	{
		scfglex_feat_vals[scfglex_num_feats] = strtod(yytext, NULL);
		++scfglex_num_feats;
		BEGIN(FEATS);
		}
<FEATVAL>.	{
		std::cerr << "Grammar " << scfglex_fname << " line " << lex_line << ": unexpected input in feature value: " << yytext << std::endl;
		abort();
		}
<FEATS>{REAL} 	{
		scfglex_feat_ids[scfglex_num_feats] = scfglex_phrase_fnames[scfglex_num_feats];
		scfglex_feat_vals[scfglex_num_feats] = strtod(yytext, NULL);
		++scfglex_num_feats;
		}
<FEATS>.	{
		std::cerr << "Grammar " << scfglex_fname << " line " << lex_line << " unexpected input in features: " << yytext << std::endl;
		abort();
		}
<ALIGNS>[0-9]+-[0-9]+	{
                int i = 0;
		int a = 0;
		int b = 0;
		while (i < yyleng) {
		  char c = yytext[i];
		  if (c == '-') break;
		  a *= 10;
		  a += c - '0';
		  ++i;
		}
		++i;
		while (i < yyleng) {
		  b *= 10;
		  b += yytext[i] - '0';
		  ++i;
		}
		scfglex_als[scfglex_num_als++]=AlignmentPoint(a,b);
		}
<ALIGNS>[ \t]	;
<ALIGNS>.	{
		std::cerr << "Grammar " << scfglex_fname << " line " << lex_line << ": unexpected input in alignment: " << yytext << std::endl;
		abort();
		}
%%

#include "filelib.h"

void RuleLexer::ReadRules(std::istream* in, RuleLexer::RuleCallback func, const std::string& fname, void* extra) {
  if (scfglex_phrase_fnames.empty()) {
    scfglex_phrase_fnames.resize(100);
    for (int i = 0; i < scfglex_phrase_fnames.size(); ++i) {
      std::ostringstream os;
      os << "PhraseModel_" << i;
      scfglex_phrase_fnames[i] = FD::Convert(os.str());
    }
  }
  lex_line = 1;
  scfglex_fname = fname;
  scfglex_stream = in;
  rule_callback_extra = extra,
  rule_callback = func;
  yylex();
}

