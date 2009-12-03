#ifndef _ERROR_SURFACE_H_
#define _ERROR_SURFACE_H_

#include <vector>
#include <string>

#include "scorer.h"

class Score;

struct ErrorSegment {
  double x;
  Score* delta;
  ErrorSegment() : x(0), delta(NULL) {}
};

class ErrorSurface : public std::vector<ErrorSegment> {
 public:
  ~ErrorSurface();
  void Serialize(std::string* out) const;
  void Deserialize(ScoreType type, const std::string& in);
};

#endif
