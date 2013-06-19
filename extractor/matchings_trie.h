#ifndef _MATCHINGS_TRIE_
#define _MATCHINGS_TRIE_

#include <memory>
#include <unordered_map>

#include "phrase.h"
#include "phrase_location.h"

using namespace std;

namespace extractor {

/**
 * Trie node containing all the occurrences of the corresponding phrase in the
 * source data.
 */
struct TrieNode {
  TrieNode(shared_ptr<TrieNode> suffix_link = shared_ptr<TrieNode>(),
           Phrase phrase = Phrase(),
           PhraseLocation matchings = PhraseLocation()) :
      suffix_link(suffix_link), phrase(phrase), matchings(matchings) {}

  // Adds a trie node as a child of the current node.
  void AddChild(int key, shared_ptr<TrieNode> child_node) {
    children[key] = child_node;
  }

  // Checks if a child exists for a given key.
  bool HasChild(int key) {
    return children.count(key);
  }

  // Gets the child corresponding to the given key.
  shared_ptr<TrieNode> GetChild(int key) {
    return children[key];
  }

  shared_ptr<TrieNode> suffix_link;
  Phrase phrase;
  PhraseLocation matchings;
  unordered_map<int, shared_ptr<TrieNode>> children;
};

/**
 * Trie containing all the phrases that can be obtained from a sentence.
 */
class MatchingsTrie {
 public:
  MatchingsTrie();

  virtual ~MatchingsTrie();

  // Returns the root of the trie.
  shared_ptr<TrieNode> GetRoot() const;

 private:
  // Recursively deletes a subtree of the trie.
  void DeleteTree(shared_ptr<TrieNode> root);

  shared_ptr<TrieNode> root;
};

} // namespace extractor

#endif
