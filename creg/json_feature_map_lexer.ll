%option nounput
%{

#include "json_feature_map_lexer.h"
#include "fdict.h"
#include "fast_sparse_vector.h"

#define YY_DECL int json_fmap_yylex (void)
#undef YY_INPUT
#define YY_INPUT(buf, result, max_size) (result = jfmap_stream->read(buf, max_size).gcount())
#define YY_SKIP_YYWRAP 1
int yywrap() { return 1; }

JSONFeatureMapLexer::FeatureMapCallback json_fmap_callback = NULL;
void* json_fmap_callback_extra = NULL;
std::istream* jfmap_stream = NULL;
bool fl = true;
unsigned spos = 0;
char featname[16000];
#define MAX_FEATS 20000
std::pair<int, float> featmap[MAX_FEATS];
unsigned curfeat = 0;
std::string instid;

inline unsigned unicode_escape_to_utf8(uint16_t w1, uint16_t w2, char* putf8) {
  uint32_t cp;
  if((w1 & 0xfc00) == 0xd800) {
    if((w2 & 0xfc00) == 0xdc00) {
      cp = 0x10000 + (((static_cast<uint32_t>(w1) & 0x3ff) << 10) | (w2 & 0x3ff));
    } else {
      abort();
    }
  } else {
    cp = w1;
  }
  
  
  if(cp < 0x80) {
    putf8[0] = static_cast<char>(cp);
    return 1;
  } else if(cp < 0x0800) {
    putf8[0] = 0xc0 | ((cp >> 6) & 0x1f);
    putf8[1] = 0x80 | (cp & 0x3f);
    return 2;
  } else if(cp < 0x10000) {
    putf8[0] = 0xe0 | ((cp >> 6) & 0x0f);
    putf8[1] = 0x80 | ((cp >> 6) & 0x3f);
    putf8[2] = 0x80 | (cp & 0x3f);
    return 3;
  } else if(cp < 0x1fffff) {
    putf8[0] = 0xf0 | ((cp >> 18) & 0x07);
    putf8[1] = 0x80 | ((cp >> 12) & 0x3f);
    putf8[2] = 0x80 | ((cp >> 6) & 0x3f);
    putf8[3] = 0x80 | (cp & 0x3f);
    return 4;
  } else {
    abort();
  } 
  return 0;
}

%}

ID [^ \t\n\r]+
HEX_D [a-fA-F0-9]
INT [-]?[0-9]+
DOUBLE {INT}((\.[0-9]+)?([eE][-+]?[0-9]+)?)
WS [ \t\r\n]
LCB [{]
RCB [}]
UNESCAPED_CH [^\"\\\b\n\r\f\t]

%x JSON PREVAL STRING JSONVAL POSTVAL DOUBLE
%%

<INITIAL>{ID}                            { instid = yytext; BEGIN(JSON); }

<JSON>{WS}*{LCB}{WS}*                    { BEGIN(PREVAL); }

<JSON>{WS}*{LCB}{WS}*{RCB}\n*            {const SparseVector<float> x;
                                         json_fmap_callback(instid, x, json_fmap_callback_extra);
                                         curfeat = 0;
                                         BEGIN(INITIAL);}

<PREVAL>\"                               { BEGIN(STRING); spos=0; }

<STRING>\"                               { featname[spos] = 0;
                                           featmap[curfeat].first = FD::Convert(featname);
                                           BEGIN(JSONVAL);
                                         }
<STRING>{UNESCAPED_CH}                   { featname[spos++] = yytext[0]; }
<STRING>\\\"                             { featname[spos++] = '"'; }
<STRING>\\\\                             { featname[spos++] = '\\'; }
<STRING>\\\/                             { featname[spos++] = '/'; }
<STRING>\\b                              { }
<STRING>\\f                              { }
<STRING>\\n                              { }
<STRING>\\r                              { }
<STRING>\\t                              { }
<STRING>\\u{HEX_D}{HEX_D}{HEX_D}{HEX_D}  { uint16_t hex = strtol(&yytext[2], NULL, 16);
                                           spos += unicode_escape_to_utf8(hex, 0, &featname[spos++])-1;
                                         }

<JSONVAL>{WS}*:{WS}*                     { BEGIN(DOUBLE); }
<DOUBLE>{DOUBLE}                         { featmap[curfeat++].second = strtod(yytext, 0);
                                           BEGIN(POSTVAL); }

<POSTVAL>{WS}*,{WS}*                     { BEGIN(PREVAL); }
<POSTVAL>{WS}*{RCB}\n*                   {
                                           const SparseVector<float> x(&featmap[0], &featmap[curfeat]);
                                           json_fmap_callback(instid, x, json_fmap_callback_extra);
                                           curfeat = 0;
                                           BEGIN(INITIAL);
                                         }

<PREVAL,POSTVAL,DOUBLE,JSONVAL,INITIAL>. { std::cerr << "bad input: " << yytext << std::endl; abort(); }

%%

void JSONFeatureMapLexer::ReadRules(std::istream* in, FeatureMapCallback func, void* extra) {
  json_fmap_callback = func;
  json_fmap_callback_extra = extra;
  jfmap_stream = in;
  json_fmap_yylex();
}

#if 0
void cb(const std::string& id, const SparseVector<float>& fmap, void* extra) {
  (void) extra;
  static int cc = 0;
  cc++;
}

int main() {
  JSONFeatureMapLexer::ReadRules(&std::cin, cb, NULL);
}
#endif
