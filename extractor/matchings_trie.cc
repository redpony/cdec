#include "matchings_trie.h"

void MatchingsTrie::Reset() {
  // TODO(pauldb): This is probably memory leaking because of the suffix links.
  // Check if it's true and free the memory properly.
  root.reset(new TrieNode());
}

shared_ptr<TrieNode> MatchingsTrie::GetRoot() const {
  return root;
}
