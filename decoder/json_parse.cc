#include "json_parse.h"

#include <string>
#include <iostream>

using namespace std;

static const char *json_hex_chars = "0123456789abcdef";

void JSONParser::WriteEscapedString(const string& in, ostream* out) {
  int pos = 0;
  int start_offset = 0;
  unsigned char c = 0;
  (*out) << '"';
  while(pos < in.size()) {
    c = in[pos];
    switch(c) {
    case '\b':
    case '\n':
    case '\r':
    case '\t':
    case '"':
    case '\\':
    case '/':
      if(pos - start_offset > 0)
	(*out) << in.substr(start_offset, pos - start_offset);
      if(c == '\b') (*out) << "\\b";
      else if(c == '\n') (*out) << "\\n";
      else if(c == '\r') (*out) << "\\r";
      else if(c == '\t') (*out) << "\\t";
      else if(c == '"') (*out) << "\\\"";
      else if(c == '\\') (*out) << "\\\\";
      else if(c == '/') (*out) << "\\/";
      start_offset = ++pos;
      break;
    default:
      if(c < ' ') {
        cerr << "Warning, bad character (" << static_cast<int>(c) << ") in string\n";
	if(pos - start_offset > 0)
	  (*out) << in.substr(start_offset, pos - start_offset);
	(*out) << "\\u00" << json_hex_chars[c >> 4] << json_hex_chars[c & 0xf];
	start_offset = ++pos;
      } else pos++;
    }
  }
  if(pos - start_offset > 0)
    (*out) << in.substr(start_offset, pos - start_offset);
  (*out) << '"';
}

