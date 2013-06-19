#include "util/pool.hh"

#include "util/scoped.hh"

#include <stdlib.h>

namespace util {

Pool::Pool() {
  current_ = NULL;
  current_end_ = NULL;
}

Pool::~Pool() {
  FreeAll();
}

void Pool::FreeAll() {
  for (std::vector<void *>::const_iterator i(free_list_.begin()); i != free_list_.end(); ++i) {
    free(*i);
  }
  free_list_.clear();
  current_ = NULL;
  current_end_ = NULL;
}

void *Pool::More(std::size_t size) {
  // Double until we hit 2^21 (2 MB).  Then grow in 2 MB blocks. 
  std::size_t desired_size = static_cast<size_t>(32) << std::min(static_cast<std::size_t>(16), free_list_.size());
  std::size_t amount = std::max(desired_size, size);
  uint8_t *ret = static_cast<uint8_t*>(MallocOrThrow(amount));
  free_list_.push_back(ret);
  current_ = ret + size;
  current_end_ = ret + amount;
  return ret;
}

} // namespace util
