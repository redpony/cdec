#include "config.h"

#ifdef HAVE_CMPH

#include "perfect_hash.h"

#include <cstdio>
#include <iostream>

using namespace std;

PerfectHashFunction::~PerfectHashFunction() {
  cmph_destroy(mphf_);
}

PerfectHashFunction::PerfectHashFunction(const string& fname) {
  FILE* f = fopen(fname.c_str(), "r");
  if (!f) {
    cerr << "Failed to open file " << fname << " for reading: cannot load hash function.\n";
    abort();
  }
  mphf_ = cmph_load(f);
  if (!mphf_) {
    cerr << "cmph_load failed on " << fname << "!\n";
    abort();
  }
}

size_t PerfectHashFunction::operator()(const string& key) const {
  return cmph_search(mphf_, &key[0], key.size());
}

size_t PerfectHashFunction::number_of_keys() const {
  return cmph_size(mphf_);
}

#endif
