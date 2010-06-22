/*
 * suffix_tree.h
 *
 *  Created on: May 17, 2010
 *      Author: Vlad
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
