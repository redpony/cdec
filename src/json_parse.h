#ifndef _JSON_WRAPPER_H_
#define _JSON_WRAPPER_H_

#include <iostream>
#include <cassert>
#include "JSON_parser.h"

class JSONParser {
 public:
  JSONParser() {
    init_JSON_config(&config);
    hack.mf = &JSONParser::Callback;
    config.depth = 10;
    config.callback_ctx = reinterpret_cast<void*>(this);
    config.callback = hack.cb;
    config.allow_comments = 1;
    config.handle_floats_manually = 1;
    jc = new_JSON_parser(&config);
  }
  virtual ~JSONParser() {
    delete_JSON_parser(jc);
  }
  bool Parse(std::istream* in) {
    int count = 0;
    int lc = 1;
    for (; in ; ++count) {
      int next_char = in->get();
      if (!in->good()) break;
      if (lc == '\n') { ++lc; }
      if (!JSON_parser_char(jc, next_char)) {
        std::cerr << "JSON_parser_char: syntax error, line " << lc << " (byte " << count << ")" << std::endl;
        return false;
      }
    }
    if (!JSON_parser_done(jc)) {
      std::cerr << "JSON_parser_done: syntax error\n";
      return false;
    }
    return true;
  }
  static void WriteEscapedString(const std::string& in, std::ostream* out);
 protected:
  virtual bool HandleJSONEvent(int type, const JSON_value* value) = 0;
 private:
  int Callback(int type, const JSON_value* value) {
    if (HandleJSONEvent(type, value)) return 1;
    return 0;
  }
  JSON_parser_struct* jc;
  JSON_config config;
  typedef int (JSONParser::* MF)(int type, const struct JSON_value_struct* value);
  union CBHack {
    JSON_parser_callback cb;
    MF mf;
  } hack;
};

#endif
