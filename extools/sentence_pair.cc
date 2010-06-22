#include "sentence_pair.h"

#include <queue>
#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <set>

#include "tdict.h"
#include "wordid.h"
#include "array2d.h"

using namespace std;

namespace {
  inline bool IsWhitespace(char c) { return c == ' ' || c == '\t'; }

  inline void SkipWhitespace(const char* buf, int* ptr) {
    while (buf[*ptr] && IsWhitespace(buf[*ptr])) { ++(*ptr); }
  }
}

void AnnotatedParallelSentence::Reset() {
  f.clear();
  e.clear();
  e_aligned.clear();
  f_aligned.clear();
  aligns_by_fword.clear();
  aligned.clear();
  span_types.clear();
}

void AnnotatedParallelSentence::AllocateForAlignment() {
  f_len = f.size();
  e_len = e.size();
  aligned.resize(f_len, e_len, false);
  f_aligned.resize(f_len, 0);
  e_aligned.resize(e_len, 0);
  aligns_by_fword.resize(f_len);
  span_types.resize(e_len, e_len+1);
}

// read an alignment point of the form X-Y where X and Y are strings
// of digits. if permit_col is true, the right edge will be determined
// by the presence of a colon
int AnnotatedParallelSentence::ReadAlignmentPoint(const char* buf,
                                                  const int start,
                                                  const int end,
                                                  const bool permit_col,
                                                  short* a,
                                                  short* b) {
  if (end - start < 3) {
    cerr << "Alignment point badly formed: " << string(buf, start, end-start) << endl; abort();
  }
  int c = start;
  *a = 0;
  while(c < end && buf[c] != '-') {
    if (buf[c] < '0' || buf[c] > '9') {
      cerr << "Alignment point badly formed: " << string(buf, start, end-start) << endl;
      abort();
    }
    (*a) *= 10;
    (*a) += buf[c] - '0';
    ++c;
  }
  ++c;
  if (c >= end) {
    cerr << "Alignment point badly formed: " << string(buf, start, end-start) << endl; abort();
  }
  (*b) = 0;
  while(c < end && (!permit_col || (permit_col && buf[c] != ':'))) {
    if (buf[c] < '0' || buf[c] > '9') {
      cerr << "Alignment point badly formed: " << string(buf, start, end-start) << endl;
      abort();
    }
    (*b) *= 10;
    (*b) += buf[c] - '0';
    ++c;
  }
  return c;
}

void AnnotatedParallelSentence::ParseAlignmentPoint(const char* buf, int start, int end) {
  short a, b;
  ReadAlignmentPoint(buf, start, end, false, &a, &b);
  assert(a < f_len);
  assert(b < e_len);
  aligned(a,b) = true;
  ++f_aligned[a];
  ++e_aligned[b];
  aligns_by_fword[a].push_back(make_pair(a,b));
  // cerr << a << " " << b << endl;
}

void AnnotatedParallelSentence::ParseSpanLabel(const char* buf, int start, int end) {
  short a,b;
  int c = ReadAlignmentPoint(buf, start, end, true, &a, &b) + 1;
  if (buf[c-1] != ':' || c >= end) {
    cerr << "Span badly formed: " << string(buf, start, end-start) << endl; abort();
  }
  // cerr << a << " " << b << " " << string(buf,c,end-c) << endl;
  span_types(a,b).push_back(-TD::Convert(string(buf, c, end-c)));
}

// INPUT FORMAT
// ein haus ||| a house ||| 0-0 1-1 ||| 0-0:DT 1-1:NN 0-1:NP
void AnnotatedParallelSentence::ParseInputLine(const char* buf) {
  Reset();
  int ptr = 0;
  SkipWhitespace(buf, &ptr);
  int start = ptr;
  int state = 0;  // 0 = French, 1 = English, 2 = Alignment, 3 = Spans
  while(char c = buf[ptr]) {
    if (!IsWhitespace(c)) { ++ptr; continue; } else {
      if (ptr - start == 3 && buf[start] == '|' && buf[start+1] == '|' && buf[start+2] == '|') {
        ++state;
        if (state == 4) { cerr << "Too many fields (ignoring):\n  " << buf << endl; return; }
        if (state == 2) {
          // cerr << "FLEN=" << f->size() << " ELEN=" << e->size() << endl;
          AllocateForAlignment();
        }
        SkipWhitespace(buf, &ptr);
        start = ptr;
        continue;
      }
      switch (state) {
        case 0:  f.push_back(TD::Convert(string(buf, start, ptr-start))); break;
        case 1:  e.push_back(TD::Convert(string(buf, start, ptr-start))); break;
        case 2:  ParseAlignmentPoint(buf, start, ptr); break;
        case 3:  ParseSpanLabel(buf, start, ptr); break;
        default: cerr << "Can't happen\n"; abort();
      }
      SkipWhitespace(buf, &ptr);
      start = ptr;
    }
  }
  if (ptr > start) {
    switch (state) {
      case 0:  f.push_back(TD::Convert(string(buf, start, ptr-start))); break;
      case 1:  e.push_back(TD::Convert(string(buf, start, ptr-start))); break;
      case 2:  ParseAlignmentPoint(buf, start, ptr); break;
      case 3:  ParseSpanLabel(buf, start, ptr); break;
      default: cerr << "Can't happen\n"; abort();
    }
  }
  if (state < 2) {
    cerr << "Not enough fields: " << buf << endl;
    abort();
  }
  if (e.empty() || f.empty()) {
    cerr << "Sentences must not be empty: " << buf << endl;
  }
}

