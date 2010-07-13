#include "striped_grammar.h"

#include <iostream>

#include "sentence_pair.h"

using namespace std;

namespace {
  inline bool IsWhitespace(char c) { return c == ' ' || c == '\t'; }

  inline void SkipWhitespace(const char* buf, int* ptr) {
    while (buf[*ptr] && IsWhitespace(buf[*ptr])) { ++(*ptr); }
  }
}

void RuleStatistics::ParseRuleStatistics(const char* buf, int start, int end) {
  int ptr = start;
  counts.clear();
  aligns.clear();
  while (ptr < end) {
    SkipWhitespace(buf, &ptr);
    int vstart = ptr;
    while(ptr < end && buf[ptr] != '=') ++ptr;
    assert(buf[ptr] == '=');
    assert(ptr > vstart);
    if (buf[vstart] == 'A' && buf[vstart+1] == '=') {
      ++ptr;
      while (ptr < end && !IsWhitespace(buf[ptr])) {
        while(ptr < end && buf[ptr] == ',') { ++ptr; }
        assert(ptr < end);
        vstart = ptr;
        while(ptr < end && buf[ptr] != ',' && !IsWhitespace(buf[ptr])) { ++ptr; }
        if (ptr > vstart) {
          short a, b;
          AnnotatedParallelSentence::ReadAlignmentPoint(buf, vstart, ptr, false, &a, &b);
          aligns.push_back(make_pair(a,b));
        }
      }
    } else {
      int name = FD::Convert(string(buf,vstart,ptr-vstart));
      ++ptr;
      vstart = ptr;
      while(ptr < end && !IsWhitespace(buf[ptr])) { ++ptr; }
      assert(ptr > vstart);
      counts.set_value(name, strtod(buf + vstart, NULL));
    }
  }
}

ostream& operator<<(ostream& os, const RuleStatistics& s) {
  bool needspace = false;
  for (SparseVector<float>::const_iterator it = s.counts.begin(); it != s.counts.end(); ++it) {
    if (needspace) os << ' '; else needspace = true;
    os << FD::Convert(it->first) << '=' << it->second;
  }
  if (s.aligns.size() > 0) {
    os << " A=";
    needspace = false;
    for (int i = 0; i < s.aligns.size(); ++i) {
      if (needspace) os << ','; else needspace = true;
      os << s.aligns[i].first << '-' << s.aligns[i].second;
    }
  }
  return os;
}

