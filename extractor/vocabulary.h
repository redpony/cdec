#ifndef _VOCABULARY_H_
#define _VOCABULARY_H_

#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

class Vocabulary {
 public:
  virtual ~Vocabulary();

  virtual int GetTerminalIndex(const string& word);

  int GetNonterminalIndex(int position);

  bool IsTerminal(int symbol);

  virtual string GetTerminalValue(int symbol);

  int Size();

 private:
  unordered_map<string, int> dictionary;
  vector<string> words;
};

#endif
