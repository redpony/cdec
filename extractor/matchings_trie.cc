#include "matchings_trie.h"

namespace extractor {

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
    if (root->suffix_link != NULL) {
      root->suffix_link.reset();
    }
    root.reset();
  }
}

} // namespace extractor
