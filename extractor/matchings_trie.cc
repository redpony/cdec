#include "matchings_trie.h"

void MatchingsTrie::Reset() {
  ResetTree(root);
  root = make_shared<TrieNode>();
}

shared_ptr<TrieNode> MatchingsTrie::GetRoot() const {
  return root;
}

void MatchingsTrie::ResetTree(shared_ptr<TrieNode> root) {
  if (root != NULL) {
    for (auto child: root->children) {
      ResetTree(child.second);
    }
    root.reset();
  }
}
