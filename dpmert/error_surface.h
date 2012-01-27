#ifndef _ERROR_SURFACE_H_
#define _ERROR_SURFACE_H_

#include <vector>
#include <string>

#include "ns.h"

class Score;

struct ErrorSegment {
  double x;
  SufficientStats delta;
  ErrorSegment() : x(0), delta() {}
};

class ErrorSurface : public std::vector<ErrorSegment> {
 public:
  ~ErrorSurface();
  void Serialize(std::string* out) const;
  void Deserialize(const std::string& in);
};

#endif
