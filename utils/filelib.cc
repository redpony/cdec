#include "filelib.h"

#include <unistd.h>
#include <sys/stat.h>

using namespace std;

bool FileExists(const std::string& fn) {
  struct stat info;
  int s = stat(fn.c_str(), &info);
  return (s==0);
}

bool DirectoryExists(const string& dir) {
  if (access(dir.c_str(),0) == 0) {
    struct stat status;
    stat(dir.c_str(), &status);
    if (status.st_mode & S_IFDIR) return true;
  }
  return false;
}

