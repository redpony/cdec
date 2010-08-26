#ifndef _SENTENCE_PAIR_H_
#define _SENTENCE_PAIR_H_

#include <map>
#include <utility>
#include <vector>
#include <boost/tuple/tuple.hpp>
#include "wordid.h"
#include "array2d.h"

// represents a parallel sentence with a word alignment and category
// annotations over subspans (currently in terms of f)
// you should read one using ParseInputLine and then use the public
// member variables to query things about it
struct AnnotatedParallelSentence {
  // read annotated parallel sentence from string
  void ParseInputLine(const char* buf);

  std::vector<WordID> f, e;  // words in f and e

  // word alignment information
  std::vector<int> e_aligned, f_aligned; // counts the number of times column/row x is aligned
  Array2D<bool> aligned;
  std::vector<std::vector<std::pair<short, short> > > aligns_by_fword;

  // span type information
  std::map< boost::tuple<short,short,short,short>, std::vector<WordID> > span_types;
  // span_types(i,j,k,l) is the list of category span (i,j) in source and (k,l) in the target language.

  int f_len, e_len;

  void Align(const short a, const short b);
  void AllocateForAlignment();

  static int ReadAlignmentPoint(const char* buf, int start, int end, bool permit_col, short* a, short* b, short* c, short* d);

 private:
  void Reset();
  void ParseAlignmentPoint(const char* buf, int start, int end);
  void ParseSpanLabel(const char* buf, int start, int end);
};

#endif
