/*
 * suffix_tree.h
 *
 *  Created on: May 17, 2010
 *      Author: Vlad

NOTE (graehl): this seems to be a (forward) trie of the suffixes (of sentences).
so O(m*n^2) for m sentences of length n.

For a real suffix tree (linear size/time), see:
http://en.wikipedia.org/wiki/Suffix_tree
http://www.cs.helsinki.fi/u/ukkonen/SuffixT1withFigs.pdf

 */

#ifndef SUFFIX_TREE_H_
#define SUFFIX_TREE_H_

#include <string>
#include <map>
#include <vector>

template <class T>
class Node {
	public:
		std::map<T, Node> edge_list_;
		int InsertPath(const std::vector<T>& p, int start, int end);
		const Node* Extend(const T& e) const {
			typename std::map<T, Node>::const_iterator it = edge_list_.find(e);
			if (it == edge_list_.end()) return NULL;
			return &it->second;
		}
};

bool DEBUG = false;

template <class T>
int Node<T>::InsertPath(const std::vector<T>& p, int start, int end){
	Node* currNode = this;
	for(int i=start;i<= end; i++ ) {
		currNode = &(currNode->edge_list_)[p[i]];
	}
	return 1;
}

#endif /* SUFFIX_TRIE_H_ */
