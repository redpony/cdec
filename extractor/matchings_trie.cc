#include "matchings_trie.h"

namespace extractor {

MatchingsTrie::MatchingsTrie() {
  root = make_shared<TrieNode>();
}

MatchingsTrie::~MatchingsTrie() {
  DeleteTree(root);
}

shared_ptr<TrieNode> MatchingsTrie::GetRoot() const {
  return root;
}

void MatchingsTrie::DeleteTree(shared_ptr<TrieNode> root) {
  if (root != NULL) {
    for (auto child: root->children) {
      DeleteTree(child.second);
    }
    if (root->suffix_link != NULL) {
      root->suffix_link.reset();
    }
    root.reset();
  }
}

} // namespace extractor
