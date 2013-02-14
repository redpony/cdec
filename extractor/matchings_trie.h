#ifndef _MATCHINGS_TRIE_
#define _MATCHINGS_TRIE_

#include <memory>
#include <unordered_map>

#include "phrase.h"
#include "phrase_location.h"

using namespace std;

struct TrieNode {
  TrieNode(shared_ptr<TrieNode> suffix_link = shared_ptr<TrieNode>(),
           Phrase phrase = Phrase(),
           PhraseLocation matchings = PhraseLocation()) :
      suffix_link(suffix_link), phrase(phrase), matchings(matchings) {}

  void AddChild(int key, shared_ptr<TrieNode> child_node) {
    children[key] = child_node;
  }

  bool HasChild(int key) {
    return children.count(key);
  }

  shared_ptr<TrieNode> GetChild(int key) {
    return children[key];
  }

  shared_ptr<TrieNode> suffix_link;
  Phrase phrase;
  PhraseLocation matchings;
  unordered_map<int, shared_ptr<TrieNode> > children;
};

class MatchingsTrie {
 public:
  void Reset();
  shared_ptr<TrieNode> GetRoot() const;

 private:
  void ResetTree(shared_ptr<TrieNode> root);

  shared_ptr<TrieNode> root;
};

#endif
