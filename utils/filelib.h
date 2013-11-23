#ifndef _FILELIB_H_
#define _FILELIB_H_

#include <cassert>
#include <string>
#include <iostream>
#include <cstdlib>
#include <boost/shared_ptr.hpp>
#include <stdexcept>
#include "gzstream.h"
#include "null_deleter.h"

bool FileExists(const std::string& file_name);
bool DirectoryExists(const std::string& dir_name);
void MkDirP(const std::string& dir_name);

// reads from standard in if filename is -
// uncompresses if file ends with .gz
// otherwise, reads from a normal file

template <class Stream>
struct BaseFile {
  typedef Stream S;
  typedef boost::shared_ptr<Stream> PS;
  void Reset() {
    ps_.reset();
  }
  bool is_null() const { return !ps_; }
  operator bool() const {
    return ps_.get();
  }
  S* stream() { return ps_.get(); }
  S* operator->() { return ps_.get(); } // compat with old ReadFile * -> new Readfile. remove?
  S &operator *() const { return get(); }
  S &get() const { return *ps_; }
  bool is_std() {
    return filename_=="-";
  }
  std::string filename_;
protected:
  void error(std::string const& reason,std::string const& filename) {
    throw std::runtime_error("File "+filename+" - "+reason);
  }

  PS ps_;
  static bool EndsWith(const std::string& f, const std::string& suf) {
    return (f.size() > suf.size()) && (f.rfind(suf) == f.size() - suf.size());
  }
};

class ReadFile : public BaseFile<std::istream> {
 public:
  ReadFile() {  }
  explicit ReadFile(const std::string& filename) {
    Init(filename);
  }
  void Init(const std::string& filename) {
    filename_=filename;
    if (is_std()) {
      ps_=PS(&std::cin,null_deleter());
    } else {
      if (!FileExists(filename)) {
        std::cerr << "File does not exist: " << filename << std::endl;
        error(filename," couldn't read nonexistant file.");
        abort();
      }
      char const* file=filename_.c_str(); // just in case the gzstream keeps using the filename for longer than the constructor, e.g. inflateReset2.  warning in valgrind that I'm hoping will disappear - it makes no sense.
      ps_=PS(EndsWith(filename, ".gz") ?
                static_cast<std::istream*>(new igzstream(file)) :
             static_cast<std::istream*>(new std::ifstream(file)));
      if (!*ps_) {
        std::cerr << "Failed to open " << filename << std::endl;
        error(filename," open for reading failed.");
        abort();
      }
    }
  }
  void ReadAll(std::string& s) {
    getline(*stream(), s, (char) EOF);
    if (s.size() > 0) s.resize(s.size()-1);
  }
};

class WriteFile : public BaseFile<std::ostream> {
 public:
  WriteFile() {}
  explicit WriteFile(std::string const& filename) { Init(filename); }
  void Init(const std::string& filename) {
    filename_=filename;
    if (is_std()) {
      ps_=PS(&std::cout,null_deleter());
    } else {
      char const* file=filename_.c_str(); // just in case the gzstream keeps using the filename for longer than the constructor, e.g. inflateReset2.  warning in valgrind that I'm hoping will disappear - it makes no sense.
      ps_=PS(EndsWith(filename, ".gz") ?
                static_cast<std::ostream*>(new ogzstream(file)) :
                static_cast<std::ostream*>(new std::ofstream(file)));
      if (!*ps_) {
        std::cerr << "Failed to open " << filename << std::endl;
        error(filename," open for writing failed.");
        abort();
      }
    }
  }
  ~WriteFile() {
    if (ps_)
      get() << std::flush;
  }
};

inline void CopyFile(std::istream &in,std::ostream &out) {
  out << in.rdbuf();
}

inline void CopyFile(std::string const& inf,std::ostream &out) {
  ReadFile r(inf);
  CopyFile(*r,out);
}

void CopyFile(std::string const& inf,std::string const& outf);

#endif
