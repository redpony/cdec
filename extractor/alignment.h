#ifndef _ALIGNMENT_H_
#define _ALIGNMENT_H_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace std;

namespace extractor {

/**
 * Data structure storing the word alignments for a parallel corpus.
 */
class Alignment {
 public:
  // Reads alignment from text file.
  Alignment(const string& filename);

  // Returns the alignment for a given sentence.
  virtual vector<pair<int, int> > GetLinks(int sentence_index) const;

  // Writes alignment to file in binary format.
  void WriteBinary(const fs::path& filepath);

  virtual ~Alignment();

 protected:
  Alignment();

 private:
  vector<vector<pair<int, int> > > alignments;
};

} // namespace extractor

#endif
