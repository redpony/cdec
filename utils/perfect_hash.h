#ifndef _PERFECT_HASH_MAP_H_
#define _PERFECT_HASH_MAP_H_

#include <vector>
#include <boost/utility.hpp>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_CMPH
#include "cmph.h"
#endif

class PerfectHashFunction : boost::noncopyable {
 public:
  explicit PerfectHashFunction(const std::string& fname);
  ~PerfectHashFunction();
  size_t operator()(const std::string& key) const;
  size_t number_of_keys() const;
 private:
#ifdef HAVE_CMPH
  cmph_t *mphf_;
#endif
};

#endif
