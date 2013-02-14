#ifndef _ALIGNMENT_H_
#define _ALIGNMENT_H_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace std;

class Alignment {
 public:
  Alignment(const string& filename);

  virtual vector<pair<int, int> > GetLinks(int sentence_index) const;

  void WriteBinary(const fs::path& filepath);

  virtual ~Alignment();

 protected:
  Alignment();

 private:
  vector<vector<pair<int, int> > > alignments;
};

#endif
