/*
 * srl_sentence.h
 *
 *  Created on: May 26, 2013
 *      Author: junhuili
 */

#ifndef SRL_SENTENCE_H_
#define SRL_SENTENCE_H_

#include "tree.h"

#include <vector>

using namespace std;

struct SArgument{
	SArgument(const char* pszRole, int iBegin, int iEnd, float fProb) {
		m_pszRole = new char[strlen(pszRole) + 1];
		strcpy(m_pszRole, pszRole);
		m_iBegin = iBegin;
		m_iEnd = iEnd;
		m_fProb = fProb;
		m_pTreeItem = NULL;
	}
	~SArgument() {
		delete [] m_pszRole;
	}

	void fnSetTreeItem(STreeItem *pTreeItem) {
		m_pTreeItem = pTreeItem;
		if (m_pTreeItem != NULL && m_pTreeItem->m_iBegin != -1) {
			assert(m_pTreeItem->m_iBegin == m_iBegin);
			assert(m_pTreeItem->m_iEnd == m_iEnd);
		}
	}

	char *m_pszRole; //argument rule, e.g., ARG0, ARGM-TMP
	int m_iBegin;
	int m_iEnd;  //the span of the argument, [m_iBegin, m_iEnd]
	float m_fProb; //the probability of this role,
	STreeItem *m_pTreeItem;
};

struct SPredicate{
	SPredicate(const char* pszLemma, int iPosition) {
		if (pszLemma != NULL) {
			m_pszLemma = new char[strlen(pszLemma) + 1];
			strcpy(m_pszLemma, pszLemma);
		} else
			m_pszLemma = NULL;
		m_iPosition = iPosition;
	}
	~SPredicate() {
		if (m_pszLemma != NULL)
			delete [] m_pszLemma;
		for (size_t i = 0; i < m_vecArgt.size(); i++)
			delete m_vecArgt[i];
	}
	int fnAppend(const char* pszRole, int iBegin, int iEnd) {
		SArgument *pArgt = new SArgument(pszRole, iBegin, iEnd, 1.0);
		return fnAppend(pArgt);
	}
	int fnAppend(SArgument *pArgt) {
		m_vecArgt.push_back(pArgt);
		int iPosition = m_vecArgt.size() - 1;
		return iPosition;
	}

	char *m_pszLemma; //lemma of the predicate, for Chinese, it's always as same as the predicate itself
	int m_iPosition; //the position in sentence
	vector<SArgument*> m_vecArgt; //arguments associated to the predicate
};

struct SSrlSentence{
	SSrlSentence() {
		m_pTree = NULL;
	}
	~SSrlSentence() {
		if (m_pTree != NULL)
			delete m_pTree;

		for (size_t i = 0; i < m_vecPred.size(); i++)
			delete m_vecPred[i];
	}
	int fnAppend(const char* pszLemma, int iPosition) {
		SPredicate *pPred = new SPredicate(pszLemma, iPosition);
		return fnAppend(pPred);
	}
	int fnAppend(SPredicate* pPred) {
		m_vecPred.push_back(pPred);
		int iPosition = m_vecPred.size() - 1;
		return iPosition;
	}
	int GetPredicateNum() {
		return m_vecPred.size();
	}

	SParsedTree *m_pTree;
	vector<SPredicate*> m_vecPred;
};

struct SSrlSentenceReader {
	SSrlSentenceReader(const char* pszSrlFname) {
		m_fpIn = fopen(pszSrlFname, "r");
		assert(m_fpIn != NULL);
	}
	~SSrlSentenceReader() {
		if (m_fpIn != NULL)
			fclose(m_fpIn);
	}

	inline void fnReplaceAll(std::string& str, const std::string& from, const std::string& to) {
	    size_t start_pos = 0;
	    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
	             str.replace(start_pos, from.length(), to);
	             start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
	    }
	}

	//TODO: here only considers flat predicate-argument structure
	//      i.e., no overlap among them
	SSrlSentence* fnReadNextSrlSentence() {
		vector<vector<string> > vecContent;
		if (fnReadNextContent(vecContent) == false)
			return NULL;

		SSrlSentence *pSrlSentence = new SSrlSentence();
		int iSize = vecContent.size();
		//put together syntactic text
		std::ostringstream ostr;
		for (int i = 0; i < iSize; i++) {
			string strSynSeg = vecContent[i][5]; //the 5th column is the syntactic segment
			size_t iPosition = strSynSeg.find_first_of('*');
			assert(iPosition != string::npos);
			ostringstream ostrTmp;
			ostrTmp << "(" << vecContent[i][2] << " " << vecContent[i][0] << ")"; //the 2th column is POS-tag, and the 0th column is word
			strSynSeg.replace(iPosition, 1, ostrTmp.str());
			fnReplaceAll(strSynSeg, "(", " (");
			ostr << strSynSeg;
		}
		string strSyn = ostr.str();
		pSrlSentence->m_pTree = SParsedTree::fnConvertFromString(strSyn.c_str());
		pSrlSentence->m_pTree->fnSetHeadWord();
		pSrlSentence->m_pTree->fnSetSpanInfo();

		//read predicate-argument structure
		int iNumPred = vecContent[0].size() - 8;
		for (int i = 0; i < iNumPred; i++) {
			vector<string> vecRole;
			vector<int> vecBegin;
			vector<int> vecEnd;
			int iPred = -1;
			for (int j = 0; j < iSize; j++) {
				const char* p = vecContent[j][i + 8].c_str();
				const char* q;
				if (p[0] == '(') {
					//starting position of an argument(or predicate)
					vecBegin.push_back(j);
					q = strchr(p, '*');
					assert(q != NULL);
					vecRole.push_back(vecContent[j][i + 8].substr(1, q - p - 1));
					if (vecRole.back().compare("V") == 0) {
						assert(iPred == -1);
						iPred = vecRole.size() - 1;
					}
				}
				if (p[strlen(p) - 1] == ')') {
					//end position of an argument(or predicate)
					vecEnd.push_back(j);
					assert(vecBegin.size() == vecEnd.size());
				}
			}
			assert(iPred != -1);
			SPredicate *pPred =  new SPredicate(pSrlSentence->m_pTree->m_vecTerminals[vecBegin[iPred]]->m_pszTerm, vecBegin[iPred]);
			pSrlSentence->fnAppend(pPred);
			for (size_t j = 0; j < vecBegin.size(); j++) {
				if (j == iPred)
					continue;
				pPred->fnAppend(vecRole[j].c_str(), vecBegin[j], vecEnd[j]);
				pPred->m_vecArgt.back()->fnSetTreeItem(pSrlSentence->m_pTree->fnFindNodeForSpan(vecBegin[j], vecEnd[j], false));
			}
		}
		return pSrlSentence;
	}
private:
	bool fnReadNextContent(vector<vector<string> >& vecContent) {
		vecContent.clear();
		if (feof(m_fpIn) == true)
			return false;
		char *pszLine;
		pszLine = new char[100001];
		pszLine[0] = '\0';
		int iLen;
		while (!feof(m_fpIn)) {
			fgets(pszLine, 10001, m_fpIn);
			iLen = strlen(pszLine);
			while (iLen > 0 && pszLine[iLen - 1] > 0 && pszLine[iLen -1] < 33) {
				pszLine[ iLen - 1 ] = '\0';
				iLen--;
			}
			if (iLen == 0)
				break; //end of this sentence

			vector<string> terms = SplitOnWhitespace(string(pszLine));
			assert(terms.size() > 7);
			vecContent.push_back(terms);
		}
		delete [] pszLine;
		return true;
	}
private:
	FILE *m_fpIn;
};
#endif /* SRL_SENTENCE_H_ */
