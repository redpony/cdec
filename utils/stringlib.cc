#include "stringlib.h"

#include <cstring>
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <map>

using namespace std;

void ParseTranslatorInput(const string& line, string* input, string* ref) {
  size_t hint = 0;
  if (line.find("{\"rules\":") == 0) {
    hint = line.find("}}");
    if (hint == string::npos) {
      cerr << "Syntax error: " << line << endl;
      abort();
    }
    hint += 2;
  }
  size_t pos = line.find("|||", hint);
  if (pos == string::npos) { *input = line; return; }
  ref->clear();
  *input = line.substr(0, pos - 1);
  string rline = line.substr(pos + 4);
  if (rline.size() > 0) {
    assert(ref);
    *ref = rline;
  }
}

void ProcessAndStripSGML(string* pline, map<string, string>* out) {
  map<string, string>& meta = *out;
  string& line = *pline;
  string lline = *pline;
  if (lline.find("<SEG")==0 || lline.find("<Seg")==0) {
    cerr << "Segment tags <seg> must be lowercase!\n";
    cerr << "  " << *pline << endl;
    abort();
  } 
  if (lline.find("<seg")!=0) return;
  size_t close = lline.find(">");
  if (close == string::npos) return; // error
  size_t end = lline.find("</seg>");
  string seg = Trim(lline.substr(4, close-4));
  string text = line.substr(close+1, end - close - 1);
  for (size_t i = 1; i < seg.size(); i++) {
    if (seg[i] == '=' && seg[i-1] == ' ') {
      string less = seg.substr(0, i-1) + seg.substr(i);
      seg = less; i = 0; continue;
    }
    if (seg[i] == '=' && seg[i+1] == ' ') {
      string less = seg.substr(0, i+1);
      if (i+2 < seg.size()) less += seg.substr(i+2);
      seg = less; i = 0; continue;
    }
  }
  line = Trim(text);
  if (seg == "") return;
  for (size_t i = 1; i < seg.size(); i++) {
    if (seg[i] == '=') {
      string label = seg.substr(0, i);
      string val = seg.substr(i+1);
      if (val[0] == '"') {
        val = val.substr(1);
        size_t close = val.find('"');
        if (close == string::npos) {
          cerr << "SGML parse error: missing \"\n";
          seg = "";
          i = 0;
        } else {
          seg = val.substr(close+1);
          val = val.substr(0, close);
          i = 0;
        }
      } else {
        size_t close = val.find(' ');
        if (close == string::npos) {
          seg = "";
          i = 0;
        } else {
          seg = val.substr(close+1);
          val = val.substr(0, close);
        }
      }
      label = Trim(label);
      seg = Trim(seg);
      meta[label] = val;
    }
  }
}

