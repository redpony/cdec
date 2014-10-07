/*
 * alignment.h
 *
 *  Created on: May 23, 2013
 *      Author: lijunhui
 */

#ifndef ALIGNMENT_H_
#define ALIGNMENT_H_

#include <string>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "stringlib.h"

using namespace std;

/*
 * Note:
 *      m_vec_s_align.size() may not be equal to the length of source side sentence
 *                           due to the last words may not be aligned
 *
 */
struct SAlignment {
	typedef vector<int> SingleAlign;
	SAlignment(const char* pszAlign) {
		fnInitializeAlignment(pszAlign);
	}
	~SAlignment() {

	}

	bool fnIsAligned(int i, bool s) const {
		const vector<SingleAlign>* palign;
		if (s == true)
			palign = &m_vec_s_align;
		else
			palign = &m_vec_t_align;
		if ((*palign)[i].size() == 0)
			return false;
		return true;
	}

	/*
	 * return true if [b, e] is aligned phrases on source side (if s==true) or on the target side (if s==false);
	 * return false, otherwise.
	 */
	bool fnIsAlignedPhrase(int b, int e, bool s, int *pob, int *poe) const {
		int ob, oe; //[b, e] on the other side
		if (s == true)
			fnGetLeftRightMost(b, e, m_vec_s_align, ob, oe);
		else
			fnGetLeftRightMost(b, e, m_vec_t_align, ob, oe);

		if (ob == -1) {
			if (pob != NULL) (*pob) = -1;
			if (poe != NULL) (*poe) = -1;
			return false; //no aligned word among [b, e]
		}
		if (pob != NULL) (*pob) = ob;
		if (poe != NULL) (*poe) = oe;

		int bb, be; //[b, e] back given [ob, oe] on the other side
		if (s == true)
			fnGetLeftRightMost(ob, oe, m_vec_t_align, bb, be);
		else
			fnGetLeftRightMost(ob, oe, m_vec_s_align, bb, be);

		if (bb < b || be > e)
			return false;
		return true;
	}

	bool fnIsAlignedTightPhrase(int b, int e, bool s, int *pob, int *poe) const {
		const vector<SingleAlign>* palign;
		if (s == true)
			palign = &m_vec_s_align;
		else
			palign = &m_vec_t_align;

		if ((*palign).size() <= e || (*palign)[b].size() == 0 || (*palign)[e].size() == 0)
			return false;

		return fnIsAlignedPhrase(b, e, s, pob, poe);
	}

	void fnGetLeftRightMost(int b, int e, bool s, int& ob, int& oe) const {
		if (s == true)
			fnGetLeftRightMost(b, e, m_vec_s_align, ob, oe);
		else
			fnGetLeftRightMost(b, e, m_vec_t_align, ob, oe);
	}

	/*
	 * look the translation of source[b, e] is continuous or not
	 * 1) return "Unaligned": if the source[b, e] is translated silently;
	 * 2) return "Con't": if none of target words in target[.., ..] is exclusively aligned to any word outside source[b, e]
	 * 3) return "Discon't": otherwise;
	 */
	string fnIsContinuous(int b, int e) const {
		int ob, oe;
		fnGetLeftRightMost(b, e, true, ob, oe);
		if (ob == -1) return "Unaligned";

		for (int i = ob; i <= oe; i++) {
			if (!fnIsAligned(i, false)) continue;
			const SingleAlign& a = m_vec_t_align[i];
			int j;
			for (j = 0; j < a.size(); j++)
				if (a[j] >= b && a[j] <= e) break;
			if (j == a.size()) return "Discon't";
		}
		return "Con't";
	}

	const SingleAlign* fnGetSingleWordAlign(int i, bool s) const {
		if (s == true) {
			if (i >= m_vec_s_align.size())
				return NULL;
			return &(m_vec_s_align[i]);
		} else {
			if (i >= m_vec_t_align.size())
				return NULL;
			return &(m_vec_t_align[i]);
		}
	}
private:
	void fnGetLeftRightMost(int b, int e, const vector<SingleAlign>& align, int& ob, int& oe) const {
		ob = oe = -1;
		for (int i = b; i <= e && i < align.size(); i++) {
			if (align[i].size() > 0) {
				if (align[i][0] < ob || ob == -1)
					ob = align[i][0];
				if (oe < align[i][align[i].size() - 1])
					oe = align[i][align[i].size() - 1];
			}
		}
	}
	void fnInitializeAlignment(const char* pszAlign) {
		m_vec_s_align.clear();
		m_vec_t_align.clear();

		vector<string> terms = SplitOnWhitespace(string(pszAlign));
		int si, ti;
		for (size_t i = 0; i < terms.size(); i++) {
			sscanf(terms[i].c_str(), "%d-%d", &si, &ti);

			while (m_vec_s_align.size() <= si) {
				SingleAlign sa;
				m_vec_s_align.push_back(sa);
			}
			while (m_vec_t_align.size() <= ti) {
				SingleAlign sa;
				m_vec_t_align.push_back(sa);
			}

			m_vec_s_align[si].push_back(ti);
			m_vec_t_align[ti].push_back(si);
		}

		//sort
		for (size_t i = 0; i < m_vec_s_align.size(); i++) {
			std::sort(m_vec_s_align[i].begin(), m_vec_s_align[i].end());
		}
		for (size_t i = 0; i < m_vec_t_align.size(); i++) {
			std::sort(m_vec_t_align[i].begin(), m_vec_t_align[i].end());
		}
	}

private:
	vector<SingleAlign> m_vec_s_align; //source side words' alignment
	vector<SingleAlign> m_vec_t_align; //target side words' alignment
};

struct SAlignmentReader {
	SAlignmentReader(const char *pszFname) {
		m_fpIn = fopen(pszFname, "r");
		assert(m_fpIn != NULL);
	}
	~SAlignmentReader() {
		if (m_fpIn != NULL)
			fclose(m_fpIn);
	}
	SAlignment* fnReadNextAlignment() {
		if (feof(m_fpIn) == true)
			return NULL;
		char *pszLine = new char[100001];
		pszLine[0] = '\0';
		fgets(pszLine, 10001, m_fpIn);
		int iLen = strlen(pszLine);
		if (iLen == 0)
			return NULL;
		while (iLen > 0 && pszLine[iLen - 1] > 0 && pszLine[iLen -1] < 33) {
			pszLine[ iLen - 1 ] = '\0';
			iLen--;
		}
		SAlignment *pAlign = new SAlignment(pszLine);
		delete [] pszLine;
		return pAlign;
	}
private:
	FILE *m_fpIn;
};


#endif /* ALIGNMENT_H_ */
