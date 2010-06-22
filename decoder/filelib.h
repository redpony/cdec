#ifndef _FILELIB_H_
#define _FILELIB_H_

#include <cassert>
#include <string>
#include <iostream>
#include <cstdlib>
#include "gzstream.h"

bool FileExists(const std::string& file_name);
bool DirectoryExists(const std::string& dir_name);

// reads from standard in if filename is -
// uncompresses if file ends with .gz
// otherwise, reads from a normal file
class ReadFile {
 public:
  ReadFile(const std::string& filename) :
    no_delete_on_exit_(filename == "-"),
    in_(no_delete_on_exit_ ? static_cast<std::istream*>(&std::cin) :
      (EndsWith(filename, ".gz") ?
        static_cast<std::istream*>(new igzstream(filename.c_str())) :
        static_cast<std::istream*>(new std::ifstream(filename.c_str())))) {
    if (!no_delete_on_exit_ && !FileExists(filename)) {
      std::cerr << "File does not exist: " << filename << std::endl;
      abort();
    }
    if (!*in_) {
      std::cerr << "Failed to open " << filename << std::endl;
      abort();
    }
  }
  ~ReadFile() {
    if (!no_delete_on_exit_) delete in_;
  }

  inline std::istream* stream() { return in_; }
  
 private:
  static bool EndsWith(const std::string& f, const std::string& suf) {
    return (f.size() > suf.size()) && (f.rfind(suf) == f.size() - suf.size());
  }
  const bool no_delete_on_exit_;
  std::istream* const in_;
};

class WriteFile {
 public:
  WriteFile(const std::string& filename) :
    no_delete_on_exit_(filename == "-"),
    out_(no_delete_on_exit_ ? static_cast<std::ostream*>(&std::cout) :
      (EndsWith(filename, ".gz") ?
        static_cast<std::ostream*>(new ogzstream(filename.c_str())) :
        static_cast<std::ostream*>(new std::ofstream(filename.c_str())))) {}
  ~WriteFile() {
    (*out_) << std::flush;
    if (!no_delete_on_exit_) delete out_;
  }

  inline std::ostream* stream() { return out_; }
  
 private:
  static bool EndsWith(const std::string& f, const std::string& suf) {
    return (f.size() > suf.size()) && (f.rfind(suf) == f.size() - suf.size());
  }
  const bool no_delete_on_exit_;
  std::ostream* const out_;
};

#endif
