#include "filelib.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>

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

void MkDirP(const string& dir) {
  if (DirectoryExists(dir)) return;
  if (mkdir(dir.c_str(), 0777)) {
    perror(dir.c_str());
    abort();
  }
  if (chmod(dir.c_str(), 07777)) {
    perror(dir.c_str());
    abort();
  }
}

#if 0
void CopyFile(const string& inf, const string& outf) {
  WriteFile w(outf);
  CopyFile(inf,*w);
}
#else
void CopyFile(const string& inf, const string& outf) {
  ofstream of(outf.c_str(), fstream::trunc|fstream::binary);
  ifstream in(inf.c_str(), fstream::binary);
  of << in.rdbuf();
}
#endif

