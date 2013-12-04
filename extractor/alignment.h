#ifndef _ALIGNMENT_H_
#define _ALIGNMENT_H_

#include <string>
#include <vector>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

using namespace std;

namespace extractor {

/**
 * Data structure storing the word alignments for a parallel corpus.
 */
class Alignment {
 public:
  // Reads alignment from text file.
  Alignment(const string& filename);

  // Creates empty alignment.
  Alignment();

  // Returns the alignment for a given sentence.
  virtual vector<pair<int, int>> GetLinks(int sentence_index) const;

  virtual ~Alignment();

  bool operator==(const Alignment& alignment) const;

 private:
  friend class boost::serialization::access;

  template<class Archive> void serialize(Archive& ar, unsigned int) {
    ar & alignments;
  }

  vector<vector<pair<int, int>>> alignments;
};

} // namespace extractor

#endif
