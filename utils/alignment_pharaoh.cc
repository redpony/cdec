#include "utils/alignment_pharaoh.h"

#include <set>

using namespace std;

static bool is_digit(char x) { return x >= '0' && x <= '9'; }

boost::shared_ptr<Array2D<bool> > AlignmentPharaoh::ReadPharaohAlignmentGrid(const string& al) {
  int max_x = 0;
  int max_y = 0;
  int i = 0;
  size_t pos = al.rfind(" ||| ");
  if (pos != string::npos) { i = pos + 5; }
  while (i < al.size()) {
    if (al[i] == '\n' || al[i] == '\r') break;
    int x = 0;
    while(i < al.size() && is_digit(al[i])) {
      x *= 10;
      x += al[i] - '0';
      ++i;
    }
    if (x > max_x) max_x = x;
    assert(i < al.size());
    if(al[i] != '-') {
      cerr << "BAD ALIGNMENT: " << al << endl;
      abort();
    }
    ++i;
    int y = 0;
    while(i < al.size() && is_digit(al[i])) {
      y *= 10;
      y += al[i] - '0';
      ++i;
    }
    if (y > max_y) max_y = y;
    while(i < al.size() && al[i] == ' ') { ++i; }
  }

  boost::shared_ptr<Array2D<bool> > grid(new Array2D<bool>(max_x + 1, max_y + 1));
  i = 0;
  if (pos != string::npos) { i = pos + 5; }
  while (i < al.size()) {
    if (al[i] == '\n' || al[i] == '\r') break;
    int x = 0;
    while(i < al.size() && is_digit(al[i])) {
      x *= 10;
      x += al[i] - '0';
      ++i;
    }
    assert(i < al.size());
    assert(al[i] == '-');
    ++i;
    int y = 0;
    while(i < al.size() && is_digit(al[i])) {
      y *= 10;
      y += al[i] - '0';
      ++i;
    }
    (*grid)(x, y) = true;
    while(i < al.size() && al[i] == ' ') { ++i; }
  }
  // cerr << *grid << endl;
  return grid;
}

void AlignmentPharaoh::SerializePharaohFormat(const Array2D<bool>& alignment, ostream* out) {
  bool need_space = false;
  for (int i = 0; i < alignment.width(); ++i)
    for (int j = 0; j < alignment.height(); ++j)
      if (alignment(i,j)) {
        if (need_space) (*out) << ' '; else need_space = true;
        (*out) << i << '-' << j;
      }
  (*out) << endl;
}

