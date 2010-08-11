#ifndef _B64_TOOLS_H_
#define _B64_TOOLS_H_

namespace B64 {
  bool b64decode(const unsigned char* data, const size_t insize, char* out, const size_t outsize);
  void b64encode(const char* data, const size_t size, std::ostream* out);
}

#endif
