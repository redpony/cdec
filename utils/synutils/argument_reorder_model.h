/*
 * argument_reorder_model.h
 *
 *  Created on: Dec 15, 2013
 *      Author: lijunhui
 */

#ifndef ARGUMENT_REORDER_MODEL_H_
#define ARGUMENT_REORDER_MODEL_H_

#include "alignment.h"
#include "tree.h"
#include "srl_sentence.h"


//an argument item or a predicate item (the verb itself)
struct SSRLItem{
        SSRLItem(const STreeItem *tree_item, string role):
        tree_item_(tree_item),
        role_(role) {

        }
        ~SSRLItem() {

        }
        const STreeItem *tree_item_;
        const string role_;
};

struct SPredicateItem{
	SPredicateItem(const SParsedTree *tree, const SPredicate *pred):
                pred_(pred) {
                vec_items_.reserve(pred->m_vecArgt.size() + 1);
                for (int i = 0; i < pred->m_vecArgt.size(); i++) {
                        vec_items_.push_back(new SSRLItem(pred->m_vecArgt[i]->m_pTreeItem, string(pred->m_vecArgt[i]->m_pszRole)));
                }
                vec_items_.push_back(new SSRLItem(tree->m_vecTerminals[pred->m_iPosition]->m_ptParent, string("Pred")));
                sort(vec_items_.begin(), vec_items_.end(), SortFunction);

                begin_ = vec_items_[0]->tree_item_->m_iBegin;
                end_ = vec_items_[vec_items_.size() - 1]->tree_item_->m_iEnd;
        }

        ~SPredicateItem() {
                vec_items_.clear();
        }

        static bool SortFunction (SSRLItem *i, SSRLItem* j) { return (i->tree_item_->m_iBegin < j->tree_item_->m_iBegin); }

        vector<SSRLItem*> vec_items_;
        int begin_;
        int end_;
        const SPredicate *pred_;
};

struct SArgumentReorderModel {
public:
	static string fnGetBlockOutcome(int iBegin, int iEnd, SAlignment *pAlign) {
		return pAlign->fnIsContinuous(iBegin, iEnd);
	}
	static void fnGetReorderType(SPredicateItem *pPredItem, SAlignment *pAlign, vector<string>& vecStrLeftReorder, vector<string>& vecStrRightReorder) {
    	vector<int> vecLeft, vecRight;
    	for (int i = 0; i < pPredItem->vec_items_.size(); i++) {
            const STreeItem *pCon1 = pPredItem->vec_items_[i]->tree_item_;
    		int iLeft1, iRight1;
            pAlign->fnGetLeftRightMost(pCon1->m_iBegin, pCon1->m_iEnd, true, iLeft1, iRight1);
            vecLeft.push_back(iLeft1);
            vecRight.push_back(iRight1);
    	}
    	vector<int> vecLeftPosition;
    	fnGetRelativePosition(vecLeft, vecLeftPosition);
    	vector<int> vecRightPosition;
    	fnGetRelativePosition(vecRight, vecRightPosition);

    	vecStrLeftReorder.clear();
    	vecStrRightReorder.clear();
    	for (int i = 1; i < vecLeftPosition.size(); i++) {
    		string strOutcome;
    		fnGetOutcome(vecLeftPosition[i - 1], vecLeftPosition[i], strOutcome);
    		vecStrLeftReorder.push_back(strOutcome);
    		fnGetOutcome(vecRightPosition[i - 1], vecRightPosition[i], strOutcome);
    		vecStrRightReorder.push_back(strOutcome);
    	}
	}

	/*
	 * features:
	 * f1: (left_label, right_label, parent_label)
	 * f2: (left_label, right_label, parent_label, other_right_sibling_label)
	 * f3: (left_label, right_label, parent_label, other_left_sibling_label)
	 * f4: (left_label, right_label, left_head_pos)
	 * f5: (left_label, right_label, left_head_word)
	 * f6: (left_label, right_label, right_head_pos)
	 * f7: (left_label, right_label, right_head_word)
	 * f8: (left_label, right_label, left_chunk_status)
	 * f9: (left_label, right_label, right_chunk_status)
	 * f10: (left_label, parent_label)
	 * f11: (right_label, parent_label)
	 *
	 * f1: (left_role, right_role, predicate_term)
	 * f2: (left_role, right_role, predicate_term, other_right_role)
	 * f3: (left_role, right_role, predicate_term, other_left_role)
	 * f4: (left_role, right_role, left_head_pos)
	 * f5: (left_role, right_role, left_head_word)
	 * f6: (left_role, right_role, left_syntactic_label)
	 * f7: (left_role, right_role, right_head_pos)
	 * f8: (left_role, right_role, right_head_word)
	 * f8: (left_role, right_role, right_syntactic_label)
	 * f8: (left_role, right_role, left_chunk_status)
	 * f9: (left_role, right_role, right_chunk_status)
	 * f10: (left_role, right_role, left_chunk_status)
	 * f11: (left_role, right_role, right_chunk_status)
	 * f12: (left_label, parent_label)
	 * f13: (right_label, parent_label)
	 */
	static void fnGenerateFeature(const SParsedTree *pTree, const SPredicate *pPred, const SPredicateItem *pPredItem, int iPos, const string& strBlock1, const string& strBlock2, ostringstream& ostr) {
		SSRLItem *pSRLItem1 = pPredItem->vec_items_[iPos - 1];
		SSRLItem *pSRLItem2 = pPredItem->vec_items_[iPos];
		const STreeItem *pCon1 = pSRLItem1->tree_item_;
		const STreeItem *pCon2 = pSRLItem2->tree_item_;

		string left_role = pSRLItem1->role_;
		string right_role = pSRLItem2->role_;

		string predicate_term = pTree->m_vecTerminals[pPred->m_iPosition]->m_pszTerm;

		vector<string> vec_other_right_sibling;
		for (int i = iPos + 1; i < pPredItem->vec_items_.size(); i++)
			vec_other_right_sibling.push_back(string(pPredItem->vec_items_[i]->role_));
		if (vec_other_right_sibling.size() == 0)
			vec_other_right_sibling.push_back(string("NULL"));

		vector<string> vec_other_left_sibling;
		for (int i = 0; i < iPos - 1; i++)
			vec_other_right_sibling.push_back(string(pPredItem->vec_items_[i]->role_));
		if (vec_other_left_sibling.size() == 0)
			vec_other_left_sibling.push_back(string("NULL"));


		//generate features
		//f1
		ostr << "f1=" << left_role << "_" << right_role << "_" << predicate_term;
		ostr << "f1=" << left_role << "_" << right_role;

		//f2
		for (int i = 0; i < vec_other_right_sibling.size(); i++) {
			ostr << " f2=" << left_role << "_" << right_role << "_" << predicate_term << "_" << vec_other_right_sibling[i];
			ostr << " f2=" << left_role << "_" << right_role << "_" << vec_other_right_sibling[i];
		}
		//f3
		for (int i = 0; i < vec_other_left_sibling.size(); i++) {
			ostr << " f3=" << left_role << "_" << right_role << "_" << predicate_term << "_" << vec_other_left_sibling[i];
			ostr << " f3=" << left_role << "_" << right_role << "_" << vec_other_left_sibling[i];
		}
		//f4
		ostr << " f4=" << left_role << "_" << right_role << "_" << pTree->m_vecTerminals[pCon1->m_iHeadWord]->m_ptParent->m_pszTerm;
		//f5
		ostr << " f5=" << left_role << "_" << right_role << "_" << pTree->m_vecTerminals[pCon1->m_iHeadWord]->m_pszTerm;
		//f6
		ostr << " f6=" << left_role << "_" << right_role << "_" << pCon2->m_pszTerm;
		//f7
		ostr << " f7=" << left_role << "_" << right_role << "_" << pTree->m_vecTerminals[pCon2->m_iHeadWord]->m_ptParent->m_pszTerm;
		//f8
		ostr << " f8=" << left_role << "_" << right_role << "_" << pTree->m_vecTerminals[pCon2->m_iHeadWord]->m_pszTerm;
		//f9
		ostr << " f9=" << left_role << "_" << right_role << "_" << pCon2->m_pszTerm;
		//f10
		ostr << " f10=" << left_role << "_" << right_role << "_" << strBlock1;
		//f11
		ostr << " f11=" << left_role << "_" << right_role << "_" << strBlock2;
		//f12
		ostr << " f12=" << left_role << "_" << predicate_term;
		ostr << " f12=" << left_role;
		//f13
		ostr << " f13=" << right_role << "_" << predicate_term;
		ostr << " f13=" << right_role;
	}

private:
	static void fnGetOutcome(int i1, int i2, string& strOutcome) {
		assert(i1 != i2);
		if (i1 < i2) {
			if (i2 > i1 + 1) strOutcome = string("DM");
			else strOutcome = string("M");
		} else {
			if (i1 > i2 + 1) strOutcome = string("DS");
			else strOutcome = string("S");
		}
	}

	static void fnGetRelativePosition(const vector<int>& vecLeft, vector<int>& vecPosition) {
		vecPosition.clear();

		vector<float> vec;
		for (int i = 0; i < vecLeft.size(); i++) {
			if (vecLeft[i] == -1) {
				if (i == 0)
					vec.push_back(-1);
				else
					vec.push_back(vecLeft[i-1] + 0.1);
			} else
				vec.push_back(vecLeft[i]);
		}

		for (int i = 0; i < vecLeft.size(); i++) {
			int count = 0;

			for (int j = 0; j < vecLeft.size(); j++) {
				if ( j == i) continue;
				if (vec[j] < vec[i]) {
					count++;
				} else if (vec[j] == vec[i] && j < i) {
					count++;
				}
			}
			vecPosition.push_back(count);
		}
	}
};


#endif /* ARGUMENT_REORDER_MODEL_H_ */
