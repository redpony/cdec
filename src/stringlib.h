#ifndef _STRINGLIB_H_

#include <map>
#include <vector>
#include <cctype>
#include <string>

// read line in the form of either:
//   source
//   source ||| target
// source will be returned as a string, target must be a sentence or
// a lattice (in PLF format) and will be returned as a Lattice object
void ParseTranslatorInput(const std::string& line, std::string* input, std::string* ref);
struct Lattice;
void ParseTranslatorInputLattice(const std::string& line, std::string* input, Lattice* ref);

inline const std::string Trim(const std::string& str, const std::string& dropChars = " \t") {
  std::string res = str;
  res.erase(str.find_last_not_of(dropChars)+1);
  return res.erase(0, res.find_first_not_of(dropChars));
}

inline void Tokenize(const std::string& str, char delimiter, std::vector<std::string>* res) {
  std::string s = str;
  int last = 0;
  res->clear();
  for (int i=0; i < s.size(); ++i)
    if (s[i] == delimiter) {
      s[i]=0;
      if (last != i) {
        res->push_back(&s[last]);
      }
      last = i + 1;
    }
  if (last != s.size())
    res->push_back(&s[last]);
}

inline std::string LowercaseString(const std::string& in) {
  std::string res(in.size(),' ');
  for (int i = 0; i < in.size(); ++i)
    res[i] = tolower(in[i]);
  return res;
}

inline int CountSubstrings(const std::string& str, const std::string& sub) {
  size_t p = 0;
  int res = 0;
  while (p < str.size()) {
    p = str.find(sub, p);
    if (p == std::string::npos) break;
    ++res;
    p += sub.size();
  }
  return res;
}

inline int SplitOnWhitespace(const std::string& in, std::vector<std::string>* out) {
  out->clear();
  int i = 0;
  int start = 0;
  std::string cur;
  while(i < in.size()) {
    if (in[i] == ' ' || in[i] == '\t') {
      if (i - start > 0)
        out->push_back(in.substr(start, i - start));
      start = i + 1;
    }
    ++i;
  }
  if (i > start)
    out->push_back(in.substr(start, i - start));
  return out->size();
}

inline void SplitCommandAndParam(const std::string& in, std::string* cmd, std::string* param) {
  cmd->clear();
  param->clear();
  std::vector<std::string> x;
  SplitOnWhitespace(in, &x);
  if (x.size() == 0) return;
  *cmd = x[0];
  for (int i = 1; i < x.size(); ++i) {
    if (i > 1) { *param += " "; }
    *param += x[i];
  }
}

void ProcessAndStripSGML(std::string* line, std::map<std::string, std::string>* out);

#endif
