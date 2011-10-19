#ifndef _PERFECT_HASH_MAP_H_
#define _PERFECT_HASH_MAP_H_

#include "config.h"

#ifndef HAVE_CMPH
#error libcmph is required to use PerfectHashFunction
#endif

#include <vector>
#include <boost/utility.hpp>
#include "cmph.h"

class PerfectHashFunction : boost::noncopyable {
 public:
  explicit PerfectHashFunction(const std::string& fname);
  ~PerfectHashFunction();
  size_t operator()(const std::string& key) const;
  size_t number_of_keys() const;
 private:
  cmph_t *mphf_;
};

#endif
