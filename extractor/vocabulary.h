#ifndef _VOCABULARY_H_
#define _VOCABULARY_H_

#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

namespace extractor {

/**
 * Data structure for mapping words to word ids.
 *
 * This strucure contains words located in the frequent collocations and words
 * encountered during the grammar extraction time. This dictionary is
 * considerably smaller than the dictionaries in the data arrays (and so is the
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

 private:
  unordered_map<string, int> dictionary;
  vector<string> words;
};

} // namespace extractor

#endif
