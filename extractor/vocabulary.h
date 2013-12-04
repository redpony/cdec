#ifndef _VOCABULARY_H_
#define _VOCABULARY_H_

#include <string>
#include <unordered_map>
#include <vector>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

using namespace std;

namespace extractor {

/**
 * Data structure for mapping words to word ids.
 *
 * This strucure contains words located in the frequent collocations and words
 * encountered during the grammar extraction time. This dictionary is
 * considerably smaller than the dictionaries in the data arays (and so is the
 * query time). Note that this is the single data structure that changes state
 * and needs to have thread safe read/write operations.
 *
 * Note: For an experiment using different vocabulary instances for each thread,
 * the running time did not improve implying that the critical regions do not
 * cause bottlenecks.
 */
class Vocabulary {
 public:
  virtual ~Vocabulary();

  // Returns the word id for the given word.
  virtual int GetTerminalIndex(const string& word);

  // Returns the id for a nonterminal located at the given position in a phrase.
  int GetNonterminalIndex(int position);

  // Checks if a symbol is a nonterminal.
  bool IsTerminal(int symbol);

  // Returns the word corresponding to the given word id.
  virtual string GetTerminalValue(int symbol);

  bool operator==(const Vocabulary& vocabulary) const;

 private:
  friend class boost::serialization::access;

  template<class Archive> void save(Archive& ar, unsigned int) const {
    ar << words;
  }

  template<class Archive> void load(Archive& ar, unsigned int) {
    ar >> words;
    for (size_t i = 0; i < words.size(); ++i) {
      dictionary[words[i]] = i;
    }
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER();

  unordered_map<string, int> dictionary;
  vector<string> words;
};

} // namespace extractor

#endif
