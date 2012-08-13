#include <iostream>
#include <cassert>

using namespace std;

namespace B64 {

static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

static void encodeblock(const unsigned char* in, ostream* os, int len) {
  char out[4];
  // cerr << len << endl;
  out[0] = cb64[ in[0] >> 2 ];
  out[1] = cb64[ ((in[0] & 0x03) << 4) | (len > 1 ? ((in[1] & 0xf0) >> 4) : static_cast<unsigned char>(0))];
  out[2] = (len > 2 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
  out[3] = (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
  os->write(out, 4);
}

void b64encode(const char* data, const size_t size, ostream* out) {
  size_t cur = 0;
  while(cur < size) {
    int len = min(static_cast<size_t>(3), size - cur);
    encodeblock(reinterpret_cast<const unsigned char*>(&data[cur]), out, len);
    cur += len;
  }
}

static void decodeblock(const unsigned char* in, unsigned char* out) {
  out[0] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
  out[1] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
  out[2] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
}

bool b64decode(const unsigned char* data, const size_t insize, char* out, const size_t outsize) {
  size_t cur = 0;
  size_t ocur = 0;
  unsigned char in[4];
  while(cur < insize) {
    assert(ocur < outsize);
    for (int i = 0; i < 4; ++i) {
      unsigned char v = data[cur];
      v = (unsigned char) ((v < 43 || v > 122) ? '\0' : cd64[ v - 43 ]);
      if (!v) {
        cerr << "B64 decode error at offset " << cur << " offending character: " << (int)data[cur] << endl;
        return false;
      }
      v = (unsigned char) ((v == '$') ? '\0' : v - 61);
      if (v) in[i] = v - 1; else in[i] = 0;
      ++cur;
    }
    decodeblock(in, reinterpret_cast<unsigned char*>(&out[ocur]));
    ocur += 3;
  }
  return true;
}

}

