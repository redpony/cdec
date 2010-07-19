#ifndef _FILELIB_H_
#define _FILELIB_H_

#include <cassert>
#include <string>
#include <iostream>
#include <cstdlib>
#include <boost/shared_ptr.hpp>
#include "gzstream.h"

bool FileExists(const std::string& file_name);
bool DirectoryExists(const std::string& dir_name);

// reads from standard in if filename is -
// uncompresses if file ends with .gz
// otherwise, reads from a normal file
struct file_null_deleter {
    void operator()(void*) const {}
};

class ReadFile {
 public:
  typedef boost::shared_ptr<std::istream> PS;
  ReadFile() {  }
  std::string filename_;
  void Init(const std::string& filename) {
    bool stdin=(filename == "-");
    if (stdin) {
      in_=PS(&std::cin,file_null_deleter());
    } else {
      if (!FileExists(filename)) {
        std::cerr << "File does not exist: " << filename << std::endl;
        abort();
      }
      filename_=filename;
      in_.reset(EndsWith(filename, ".gz") ?
                static_cast<std::istream*>(new igzstream(filename.c_str())) :
                static_cast<std::istream*>(new std::ifstream(filename.c_str())));
      if (!*in_) {
        std::cerr << "Failed to open " << filename << std::endl;
        abort();
      }
    }
  }
  void Reset() {
    in_.reset();
  }
  bool is_null() const { return !in_; }
  operator bool() const {
    return in_;
  }

  explicit ReadFile(const std::string& filename) {
    Init(filename);
  }
  ~ReadFile() {
  }

  std::istream* stream() { return in_.get(); }
  std::istream* operator->() { return in_.get(); } // compat with old ReadFile * -> new Readfile. remove?
  std::istream &get() const { return *in_; }

 private:
  static bool EndsWith(const std::string& f, const std::string& suf) {
    return (f.size() > suf.size()) && (f.rfind(suf) == f.size() - suf.size());
  }
  PS in_;
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
  std::ostream &get() const { return *out_; }

 private:
  static bool EndsWith(const std::string& f, const std::string& suf) {
    return (f.size() > suf.size()) && (f.rfind(suf) == f.size() - suf.size());
  }
  const bool no_delete_on_exit_;
  std::ostream* const out_;
};

#endif
