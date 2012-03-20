#include "sentence_pair.h"

#include <queue>
#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <set>
#include <boost/tuple/tuple_comparison.hpp>

#include "tdict.h"
#include "wordid.h"
#include "array2d.h"

using namespace std;
using namespace boost;

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
}

// read an alignment point of the form X-Y where X and Y are strings
// of digits. if permit_col is true, the right edge will be determined
// by the presence of a colon
int AnnotatedParallelSentence::ReadAlignmentPoint(const char* buf,
                                                  const int start,
                                                  const int end,
                                                  const bool permit_col,
                                                  short* a, short* b, short* c, short* d) {
  if (end - start < 3) {
    cerr << "Alignment point badly formed 1: " << string(buf, start, end-start) << endl << buf << endl;
    exit(1);
  }
  int ch = start;
  *a = 0;
  while(ch < end && buf[ch] != '-') {
    if (buf[ch] < '0' || buf[ch] > '9') {
      cerr << "Alignment point badly formed 2: " << string(buf, start, end-start) << endl << buf << endl;
      exit(1);
    }
    (*a) *= 10;
    (*a) += buf[ch] - '0';
    ++ch;
  }
  ++ch;
  if (ch >= end) {
    cerr << "Alignment point badly formed 3: " << string(buf, start, end-start) << endl << buf << endl;
    exit(1);
  }
  (*b) = 0;
  while((ch < end) && (c == 0 && (!permit_col || (permit_col && buf[ch] != ':')) || c != 0 && buf[ch] != '-')) {
    if ((buf[ch] < '0') || (buf[ch] > '9')) {
      cerr << "Alignment point badly formed 4: " << string(buf, start, end-start) << endl << buf << endl << buf[ch] << endl;
      exit(1);
    }
    (*b) *= 10;
    (*b) += buf[ch] - '0';
    ++ch;
  }
  if (c != 0)
  {
      ++ch;
      if (ch >= end) {
        cerr << "Alignment point badly formed 5: " << string(buf, start, end-start) << endl << buf << endl;
        exit(1);
      }
      (*c) = 0;
      while(ch < end && buf[ch] != '-') {
        if (buf[ch] < '0' || buf[ch] > '9') {
          cerr << "Alignment point badly formed 6: " << string(buf, start, end-start) << endl << buf << endl;
          exit(1);
        }
        (*c) *= 10;
        (*c) += buf[ch] - '0';
        ++ch;
      }
      ++ch;
      if (ch >= end) {
        cerr << "Alignment point badly formed 7: " << string(buf, start, end-start) << endl << buf << endl;
        exit(1);
      }
      (*d) = 0;
      while(ch < end && (!permit_col || (permit_col && buf[ch] != ':'))) {
        if (buf[ch] < '0' || buf[ch] > '9') {
          cerr << "Alignment point badly formed 8: " << string(buf, start, end-start) << endl << buf << endl;
          exit(1);
        }
        (*d) *= 10;
        (*d) += buf[ch] - '0';
        ++ch;
      }
  }
  return ch;
}

void AnnotatedParallelSentence::Align(const short a, const short b) {
  aligned(a,b) = true;
  ++f_aligned[a];
  ++e_aligned[b];
  aligns_by_fword[a].push_back(make_pair(a,b));
  // cerr << a << " " << b << endl;
}

void AnnotatedParallelSentence::ParseAlignmentPoint(const char* buf, int start, int end) {
  short a, b;
  ReadAlignmentPoint(buf, start, end, false, &a, &b, 0, 0);
  if (a >= f_len || b >= e_len) {
    cerr << "(" << a << ',' << b << ") is out of bounds. INPUT=\n" << buf << endl;
    exit(1);
  }
  Align(a,b);
}

void AnnotatedParallelSentence::ParseSpanLabel(const char* buf, int start, int end) {
  short a,b,c,d;
  int ch = ReadAlignmentPoint(buf, start, end, true, &a, &b, &c, &d) + 1;
  if (buf[ch-1] != ':' || ch >= end) {
    cerr << "Span badly formed: " << string(buf, start, end-start) << endl << buf << endl;
    exit(1);
  }
  if (a >= f_len || b > f_len) {
    cerr << "(" << a << ',' << b << ") is out of bounds in labeled span. INPUT=\n" << buf << endl;
    exit(1);
  }
  if (c >= e_len || d > e_len) {
    cerr << "(" << c << ',' << d << ") is out of bounds in labeled span. INPUT=\n" << buf << endl;
    exit(1);
  }
  // cerr << a << " " << b << " " << string(buf,c,end-c) << endl;
  span_types[boost::make_tuple(a,b,c,d)].push_back(-TD::Convert(string(buf, ch, end-ch)));
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
}

