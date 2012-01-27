#include "error_surface.h"

#include <cassert>
#include <sstream>

using namespace std;

ErrorSurface::~ErrorSurface() {}

void ErrorSurface::Serialize(std::string* out) const {
  const int segments = this->size();
  ostringstream os(ios::binary);
  os.write((const char*)&segments,sizeof(segments));
  for (int i = 0; i < segments; ++i) {
    const ErrorSegment& cur = (*this)[i];
    string senc;
    cur.delta.Encode(&senc);
    assert(senc.size() < 1024);
    unsigned char len = senc.size();
    os.write((const char*)&cur.x, sizeof(cur.x));
    os.write((const char*)&len, sizeof(len));
    os.write((const char*)&senc[0], len);
  }
  *out = os.str();
}

void ErrorSurface::Deserialize(const std::string& in) {
  istringstream is(in, ios::binary);
  int segments;
  is.read((char*)&segments, sizeof(segments));
  this->resize(segments);
  for (int i = 0; i < segments; ++i) {
    ErrorSegment& cur = (*this)[i];
    unsigned char len;
    is.read((char*)&cur.x, sizeof(cur.x));
    is.read((char*)&len, sizeof(len));
    string senc(len, '\0'); assert(senc.size() == len);
    is.read((char*)&senc[0], len);
    cur.delta = SufficientStats(senc);
  }
}

