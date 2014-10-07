/*
 * constituent_reorder_model.cc
 *
 *  Created on: Jul 10, 2013
 *      Author: junhuili
 */


#include <boost/program_options.hpp>

#include "alignment.h"
#include "tree.h"
#include "utility.h"
#include "tsuruoka_maxent.h"

#include <tr1/unordered_map>

using namespace std;


typedef std::tr1::unordered_map<std::string, int> Map;
typedef std::tr1::unordered_map<std::string, int>::iterator Iterator;

namespace po = boost::program_options;

inline void fnPreparingTrainingdata(const char* pszFName, int iCutoff, const char* pszNewFName) {
	SFReader *pFReader = new STxtFileReader(pszFName);
	char *pszLine = new char[ 100001 ];
	int iLen;
	Map hashPredicate;
	while (pFReader->fnReadNextLine(pszLine, &iLen)) {
		if (iLen == 0)
			continue;

		vector<string> vecTerms;
		SplitOnWhitespace(string(pszLine), &vecTerms);

		for (size_t i = 0; i < vecTerms.size() - 1; i++) {
			Iterator iter = hashPredicate.find(vecTerms[i]);
			if (iter == hashPredicate.end()) {
				hashPredicate[vecTerms[i]] = 1;

			} else {
				iter->second++;
			}
		}
	}
	delete pFReader;

	pFReader = new STxtFileReader(pszFName);
	FILE *fpOut = fopen(pszNewFName, "w");
	while (pFReader->fnReadNextLine(pszLine, &iLen)) {
		if (iLen == 0)
			continue;

		vector<string> vecTerms;
		SplitOnWhitespace(string(pszLine), &vecTerms);
		ostringstream ostr;
		for (size_t i = 0; i < vecTerms.size() - 1; i++) {
			Iterator iter = hashPredicate.find(vecTerms[i]);
			assert(iter != hashPredicate.end());
			if (iter->second >= iCutoff) {
				ostr << vecTerms[i] << " ";
			}
		}
		if (ostr.str().length() > 0) {
			ostr << vecTerms[vecTerms.size() - 1];
			fprintf(fpOut, "%s\n", ostr.str().c_str());
		}
	}
	fclose(fpOut);
	delete pFReader;


	delete [] pszLine;
}

struct SConstReorderTrainer{
	SConstReorderTrainer(const char* pszSynFname,        //source-side flattened parse tree file name
                         const char* pszAlignFname,               //alignment filename
                         const char* pszSourceFname,              //source file name
                         const char* pszTargetFname,              //target file name
                         const char* pszInstanceFname,            //training instance file name
                         const char* pszModelPrefix,              //classifier model file name prefix
                         int iClassifierType,                     //classifier type
                         int iCutoff,                             //feature count threshold
                         const char* pszOption                    //other classifier parameters (for svmlight)
                         ) {
		fnGenerateInstanceFile(pszSynFname, pszAlignFname, pszSourceFname, pszTargetFname, pszInstanceFname);

		string strInstanceLeftFname = string(pszInstanceFname) + string(".left");
		string strInstanceRightFname = string(pszInstanceFname) + string(".right");

		string strModelLeftFname = string(pszModelPrefix) + string(".left");
		string strModelRightFname = string(pszModelPrefix) + string(".right");

		fprintf(stdout, "...Training the left ordering model\n");
		fnTraining(strInstanceLeftFname.c_str(), strModelLeftFname.c_str(), iCutoff);
		fprintf(stdout, "...Training the right ordering model\n");
		fnTraining(strInstanceRightFname.c_str(), strModelRightFname.c_str(), iCutoff);
	}
	~SConstReorderTrainer() {

	}

private:

	void fnTraining(const char* pszInstanceFname, const char* pszModelFname, int iCutoff) {
		char *pszNewInstanceFName = new char[strlen(pszInstanceFname) + 50];
		if (iCutoff > 0) {
			sprintf(pszNewInstanceFName, "%s.tmp", pszInstanceFname);
			fnPreparingTrainingdata(pszInstanceFname, iCutoff, pszNewInstanceFName);
		} else {
			strcpy(pszNewInstanceFName, pszInstanceFname);
		}

		/*Zhangle_Maxent *pZhangleMaxent = new Zhangle_Maxent(NULL);
	    pZhangleMaxent->fnTrain(pszInstanceFname, "lbfgs", pszModelFname, 100, 2.0);
	    delete pZhangleMaxent;*/

		Tsuruoka_Maxent *pMaxent = new Tsuruoka_Maxent(NULL);
		pMaxent->fnTrain(pszNewInstanceFName, "l1", pszModelFname, 300);
		delete pMaxent;

		if (strcmp(pszNewInstanceFName, pszInstanceFname) != 0) {
			sprintf(pszNewInstanceFName, "rm %s.tmp", pszInstanceFname);
			system(pszNewInstanceFName);
		}
		delete [] pszNewInstanceFName;
	}

	inline bool fnIsVerbPOS(const char* pszTerm) {
		if (strcmp(pszTerm, "VV") == 0
			|| strcmp(pszTerm, "VA") == 0
			|| strcmp(pszTerm, "VC") == 0
			|| strcmp(pszTerm, "VE") == 0)
			return true;
		return false;
	}

	inline void fnGetOutcome(int iL1, int iR1, int iL2, int iR2, const SAlignment *pAlign, string& strOutcome) {
		if (iL1 == -1 && iL2 == -1)
			strOutcome = "BU"; //1. both are untranslated
		else if (iL1 == -1)
			strOutcome = "1U"; //2. XP1 is untranslated
		else if (iL2 == -1)
			strOutcome = "2U"; //3. XP2 is untranslated
		else if (iL1 == iL2 && iR2 == iR2)
			strOutcome = "SS"; //4. Have same scope
		else if (iL1 <= iL2 && iR1 >= iR2)
			strOutcome = "1C2"; //5. XP1's translation covers XP2's
		else if (iL1 >= iL2 && iR1 <= iR2)
			strOutcome = "2C1"; //6. XP2's translation covers XP1's
		else if (iR1 < iL2) {
			int i = iR1 + 1;
			/*while (i < iL2) {
				if (pAlign->fnIsAligned(i, false))
					break;
				i++;
			}*/
			if (i == iL2)
				strOutcome = "M"; //7. Monotone
			else
				strOutcome = "DM"; //8. Discontinuous monotone
		} else if (iL1 < iL2 && iL2 <= iR1 && iR1 < iR2)
			strOutcome = "OM"; //9. Overlap monotone
		else if (iR2 < iL1) {
			int i = iR2 + 1;
			/*while (i < iL1) {
				if (pAlign->fnIsAligned(i, false))
					break;
				i++;
			}*/
			if (i == iL1)
				strOutcome = "S"; //10. Swap
			else
				strOutcome = "DS"; //11. Discontinuous swap
		} else if (iL2 < iL1 && iL1 <= iR2 && iR2 < iR1)
			strOutcome = "OS"; //12. Overlap swap
		else
			assert(false);
	}

	inline void fnGetOutcome(int i1, int i2, string& strOutcome) {
		assert(i1 != i2);
		if (i1 < i2) {
			if (i2 > i1 + 1) strOutcome = string("DM");
			else strOutcome = string("M");
		} else {
			if (i1 > i2 + 1) strOutcome = string("DS");
			else strOutcome = string("S");
		}
	}

	inline void fnGetRelativePosition(const vector<int>& vecLeft, vector<int>& vecPosition) {
		vecPosition.clear();

		vector<float> vec;
		for (size_t i = 0; i < vecLeft.size(); i++) {
			if (vecLeft[i] == -1) {
				if (i == 0)
					vec.push_back(-1);
				else
					vec.push_back(vecLeft[i-1] + 0.1);
			} else
				vec.push_back(vecLeft[i]);
		}

		for (size_t i = 0; i < vecLeft.size(); i++) {
			int count = 0;

			for (size_t j = 0; j < vecLeft.size(); j++) {
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
	 */
	void fnGenerateInstance(const SParsedTree *pTree, const STreeItem *pParent, int iPos, const vector<string>& vecChunkStatus, const vector<int>& vecPosition, const vector<string>& vecSTerms, const vector<string>& vecTTerms, string& strOutcome, ostringstream& ostr) {
		STreeItem *pCon1, *pCon2;
		pCon1 = pParent->m_vecChildren[iPos - 1];
		pCon2 = pParent->m_vecChildren[iPos];

		fnGetOutcome(vecPosition[iPos-1], vecPosition[iPos], strOutcome);

		string left_label = string(pCon1->m_pszTerm);
		string right_label = string(pCon2->m_pszTerm);
		string parent_label = string(pParent->m_pszTerm);

		vector<string> vec_other_right_sibling;
		for (int i = iPos + 1; i < pParent->m_vecChildren.size(); i++)
			vec_other_right_sibling.push_back(string(pParent->m_vecChildren[i]->m_pszTerm));
		if (vec_other_right_sibling.size() == 0)
			vec_other_right_sibling.push_back(string("NULL"));
		vector<string> vec_other_left_sibling;
		for (int i = 0; i < iPos - 1; i++)
			vec_other_left_sibling.push_back(string(pParent->m_vecChildren[i]->m_pszTerm));
		if (vec_other_left_sibling.size() == 0)
			vec_other_left_sibling.push_back(string("NULL"));

		//generate features
		//f1
		ostr << "f1=" << left_label << "_" << right_label << "_" << parent_label;
		//f2
		for (int i = 0; i < vec_other_right_sibling.size(); i++)
			ostr << " f2=" << left_label << "_" << right_label << "_" << parent_label << "_" << vec_other_right_sibling[i];
		//f3
		for (int i = 0; i < vec_other_left_sibling.size(); i++)
			ostr << " f3=" << left_label << "_" << right_label << "_" << parent_label << "_" << vec_other_left_sibling[i];
		//f4
		ostr << " f4=" << left_label << "_" << right_label << "_" << pTree->m_vecTerminals[pCon1->m_iHeadWord]->m_ptParent->m_pszTerm;
		//f5
		ostr << " f5=" << left_label << "_" << right_label << "_" << vecSTerms[pCon1->m_iHeadWord];
		//f6
		ostr << " f6=" << left_label << "_" << right_label << "_" << pTree->m_vecTerminals[pCon2->m_iHeadWord]->m_ptParent->m_pszTerm;
		//f7
		ostr << " f7=" << left_label << "_" << right_label << "_" << vecSTerms[pCon2->m_iHeadWord];
		//f8
		ostr << " f8=" << left_label << "_" << right_label << "_" << vecChunkStatus[iPos - 1];
		//f9
		ostr << " f9=" << left_label << "_" << right_label << "_" << vecChunkStatus[iPos];
		//f10
		ostr << " f10=" << left_label << "_" << parent_label;
		//f11
		ostr << " f11=" << right_label << "_" << parent_label;
	}

	/*
	 * Source side (11 features):
	 * f1: the categories of XP1 and XP2 (f1_1, f1_2)
	 * f2: the head words of XP1 and XP2 (f2_1, f2_2)
	 * f3: the first and last word of XP1 (f3_f, f3_l)
	 * f4: the first and last word of XP2 (f4_f, f4_l)
	 * f5: is XP1 or XP2 the head node (f5_1, f5_2)
	 * f6: the category of the common parent
	 * Target side (6 features):
	 * f7: the first and the last word of XP1's translation (f7_f, f7_l)
	 * f8: the first and the last word of XP2's translation (f8_f, f8_l)
	 * f9: the translation of XP1's and XP2's head word (f9_1, f9_2)
	 */
	void fnGenerateInstance(const SParsedTree *pTree, const STreeItem *pParent, const STreeItem *pCon1, const STreeItem *pCon2, const SAlignment *pAlign, const vector<string>& vecSTerms, const vector<string>& vecTTerms, string& strOutcome, ostringstream& ostr) {

		int iLeft1, iRight1, iLeft2, iRight2;
		pAlign->fnGetLeftRightMost(pCon1->m_iBegin, pCon1->m_iEnd, true, iLeft1, iRight1);
		pAlign->fnGetLeftRightMost(pCon2->m_iBegin, pCon2->m_iEnd, true, iLeft2, iRight2);

		fnGetOutcome(iLeft1, iRight1, iLeft2, iRight2, pAlign, strOutcome);

		//generate features
		//f1
		ostr << "f1_1=" << pCon1->m_pszTerm << " f1_2=" << pCon2->m_pszTerm;
		//f2
		ostr << " f2_1=" << vecSTerms[pCon1->m_iHeadWord] << " f2_2" << vecSTerms[pCon2->m_iHeadWord];
		//f3
		ostr << " f3_f=" << vecSTerms[pCon1->m_iBegin] << " f3_l=" << vecSTerms[pCon1->m_iEnd];
		//f4
		ostr << " f4_f=" << vecSTerms[pCon2->m_iBegin] << " f4_l=" << vecSTerms[pCon2->m_iEnd];
		//f5
		if (pParent->m_iHeadChild == pCon1->m_iBrotherIndex)
			ostr << " f5_1=1";
		else
			ostr << " f5_1=0";
		if (pParent->m_iHeadChild == pCon2->m_iBrotherIndex)
			ostr << " f5_2=1";
		else
			ostr << " f5_2=0";
		//f6
		ostr << " f6=" << pParent->m_pszTerm;

		/*//f7
		if (iLeft1 != -1) {
			ostr << " f7_f=" << vecTTerms[iLeft1] << " f7_l=" << vecTTerms[iRight1];
		}
		if (iLeft2 != -1) {
			ostr << " f8_f=" << vecTTerms[iLeft2] << " f8_l=" << vecTTerms[iRight2];
		}

		const vector<int>* pvecTarget = pAlign->fnGetSingleWordAlign(pCon1->m_iHeadWord, true);
		string str = "";
		for (size_t i = 0; pvecTarget != NULL && i < pvecTarget->size(); i++) {
			str += vecTTerms[(*pvecTarget)[i]] + "_";
		}
		if (str.length() > 0) {
			ostr << " f9_1=" << str.substr(0, str.size()-1);
		}
		pvecTarget = pAlign->fnGetSingleWordAlign(pCon2->m_iHeadWord, true);
		str = "";
		for (size_t i = 0; pvecTarget != NULL && i < pvecTarget->size(); i++) {
			str += vecTTerms[(*pvecTarget)[i]] + "_";
		}
		if (str.length() > 0) {
			ostr << " f9_2=" << str.substr(0, str.size()-1);
		} */

	}

	void fnGetFocusedParentNodes(const SParsedTree* pTree, vector<STreeItem*>& vecFocused){
		for (size_t i = 0; i < pTree->m_vecTerminals.size(); i++) {
			STreeItem *pParent = pTree->m_vecTerminals[i]->m_ptParent;

			while (pParent != NULL) {
				//if (pParent->m_vecChildren.size() > 1 && pParent->m_iEnd - pParent->m_iBegin > 5) {
				if (pParent->m_vecChildren.size() > 1) {
					//do constituent reordering for all children of pParent
					vecFocused.push_back(pParent);
				}
				if (pParent->m_iBrotherIndex != 0) break;
				pParent = pParent->m_ptParent;
			}
		}
	}

	void fnGenerateInstanceFile(const char* pszSynFname,        //source-side flattened parse tree file name
                                const char* pszAlignFname,               //alignment filename
                                const char* pszSourceFname,              //source file name
                                const char* pszTargetFname,              //target file name
                                const char* pszInstanceFname             //training instance file name
                                ) {
		SAlignmentReader *pAlignReader = new SAlignmentReader(pszAlignFname);
		SParseReader *pParseReader = new SParseReader(pszSynFname, false);
		STxtFileReader *pTxtSReader = new STxtFileReader(pszSourceFname);
		STxtFileReader *pTxtTReader = new STxtFileReader(pszTargetFname);

		string strInstanceLeftFname = string(pszInstanceFname) + string(".left");
		string strInstanceRightFname = string(pszInstanceFname) + string(".right");

		FILE *fpLeftOut = fopen(strInstanceLeftFname.c_str(), "w");
		assert(fpLeftOut != NULL);

		FILE *fpRightOut = fopen(strInstanceRightFname.c_str(), "w");
		assert(fpRightOut != NULL);

		//read sentence by sentence
		SAlignment *pAlign;
		SParsedTree *pTree;
		char *pszLine = new char[50001];
		int iSentNum = 0;
		while ((pAlign = pAlignReader->fnReadNextAlignment()) != NULL) {
			pTree = pParseReader->fnReadNextParseTree();
			assert(pTxtSReader->fnReadNextLine(pszLine, NULL));
			vector<string> vecSTerms;
			SplitOnWhitespace(string(pszLine), &vecSTerms);
			assert(pTxtTReader->fnReadNextLine(pszLine, NULL));
			vector<string> vecTTerms;
			SplitOnWhitespace(string(pszLine), &vecTTerms);


			if (pTree != NULL) {

				vector<STreeItem*> vecFocused;
				fnGetFocusedParentNodes(pTree, vecFocused);

				for (size_t i = 0; i < vecFocused.size(); i++) {

					STreeItem *pParent = vecFocused[i];

	            	vector<int> vecLeft, vecRight;
	            	for (size_t j = 0; j < pParent->m_vecChildren.size(); j++) {
	                    STreeItem *pCon1 = pParent->m_vecChildren[j];
	            		int iLeft1, iRight1;
	                    pAlign->fnGetLeftRightMost(pCon1->m_iBegin, pCon1->m_iEnd, true, iLeft1, iRight1);
	                    vecLeft.push_back(iLeft1);
	                    vecRight.push_back(iRight1);
	            	}
	            	vector<int> vecLeftPosition;
	            	fnGetRelativePosition(vecLeft, vecLeftPosition);
	            	vector<int> vecRightPosition;
	            	fnGetRelativePosition(vecRight, vecRightPosition);

	            	vector<string> vecChunkStatus;
	            	for (size_t j = 0; j < pParent->m_vecChildren.size(); j++) {
	            		string strOutcome = pAlign->fnIsContinuous(pParent->m_vecChildren[j]->m_iBegin, pParent->m_vecChildren[j]->m_iEnd);
	            		vecChunkStatus.push_back(strOutcome);
	            	}

					for (size_t j = 1; j < pParent->m_vecChildren.size(); j++) {
						//children[j-1] vs. children[j] reordering

						string strLeftOutcome;
						ostringstream ostr;

						fnGenerateInstance(pTree, pParent, j, vecChunkStatus, vecLeftPosition, vecSTerms, vecTTerms, strLeftOutcome, ostr);

						//fprintf(stderr, "%s %s\n", ostr.str().c_str(), strLeftOutcome.c_str());
						fprintf(fpLeftOut, "%s %s\n", ostr.str().c_str(), strLeftOutcome.c_str());

						string strRightOutcome;
						fnGetOutcome(vecRightPosition[j-1], vecRightPosition[j], strRightOutcome);
						fprintf(fpRightOut, "%s LeftOrder=%s %s\n", ostr.str().c_str(), strLeftOutcome.c_str(), strRightOutcome.c_str());
					}
				}
				delete pTree;
			}

			delete pAlign;
			iSentNum++;

			if (iSentNum % 100000 == 0)
				fprintf(stderr, "#%d\n", iSentNum);
		}


		fclose(fpLeftOut);
		fclose(fpRightOut);
		delete pAlignReader;
		delete pParseReader;
		delete pTxtSReader;
		delete pTxtTReader;
		delete [] pszLine;
	}

	void fnGenerateInstanceFile2(const char* pszSynFname,        //source-side flattened parse tree file name
                                const char* pszAlignFname,               //alignment filename
                                const char* pszSourceFname,              //source file name
                                const char* pszTargetFname,              //target file name
                                const char* pszInstanceFname             //training instance file name
                                ) {
		SAlignmentReader *pAlignReader = new SAlignmentReader(pszAlignFname);
		SParseReader *pParseReader = new SParseReader(pszSynFname, false);
		STxtFileReader *pTxtSReader = new STxtFileReader(pszSourceFname);
		STxtFileReader *pTxtTReader = new STxtFileReader(pszTargetFname);

		FILE *fpOut = fopen(pszInstanceFname, "w");
		assert(fpOut != NULL);

		//read sentence by sentence
		SAlignment *pAlign;
		SParsedTree *pTree;
		char *pszLine = new char[50001];
		int iSentNum = 0;
		while ((pAlign = pAlignReader->fnReadNextAlignment()) != NULL) {
			pTree = pParseReader->fnReadNextParseTree();
			assert(pTxtSReader->fnReadNextLine(pszLine, NULL));
			vector<string> vecSTerms;
			SplitOnWhitespace(string(pszLine), &vecSTerms);
			assert(pTxtTReader->fnReadNextLine(pszLine, NULL));
			vector<string> vecTTerms;
			SplitOnWhitespace(string(pszLine), &vecTTerms);


			if (pTree != NULL) {

				vector<STreeItem*> vecFocused;
				fnGetFocusedParentNodes(pTree, vecFocused);

				for (size_t i = 0; i < vecFocused.size() && pTree->m_vecTerminals.size() > 10; i++) {

					STreeItem *pParent = vecFocused[i];

					for (size_t j = 1; j < pParent->m_vecChildren.size(); j++) {
						//children[j-1] vs. children[j] reordering

						string strOutcome;
						ostringstream ostr;

						fnGenerateInstance(pTree, pParent, pParent->m_vecChildren[j-1], pParent->m_vecChildren[j], pAlign, vecSTerms, vecTTerms, strOutcome, ostr);

						//fprintf(stderr, "%s %s\n", ostr.str().c_str(), strOutcome.c_str());
						fprintf(fpOut, "%s %s\n", ostr.str().c_str(), strOutcome.c_str());
					}
				}
				delete pTree;
			}

			delete pAlign;
			iSentNum++;

			if (iSentNum % 100000 == 0)
				fprintf(stderr, "#%d\n", iSentNum);
		}


		fclose(fpOut);
		delete pAlignReader;
		delete pParseReader;
		delete pTxtSReader;
		delete pTxtTReader;
		delete [] pszLine;
	}
};

struct SConstContTrainer{
	SConstContTrainer(const char* pszFlattenedSynFname,        //source-side flattened parse tree file name
	                  const char* pszAlignFname,               //alignment filename
	                  const char* pszSourceFname,              //source file name
	                  const char* pszTargetFname,              //target file name
	                  const char* pszInstanceFname,            //training instance file name
	                  const char* pszModelPrefix,              //classifier model file name prefix
	                  int iClassifierType,                     //classifier type
	                  int iCutoff,                             //feature count threshold
	                  const char* pszOption                    //other classifier parameters (for svmlight)
	                 ) {
		fnGenerateInstanceFile(pszFlattenedSynFname, pszAlignFname, pszSourceFname, pszTargetFname, pszInstanceFname);
		//fnTraining(pszInstanceFname, pszModelPrefix, iClassifierType, iCutoff, pszOption);
		fnTraining(pszInstanceFname, pszModelPrefix, iCutoff);
	}
	~SConstContTrainer() {

	}

private:

	void fnTraining(const char* pszInstanceFname, const char* pszModelFname, int iCutoff) {
		char *pszNewInstanceFName = new char[strlen(pszInstanceFname) + 50];
		if (iCutoff > 0) {
			sprintf(pszNewInstanceFName, "%s.tmp", pszInstanceFname);
			fnPreparingTrainingdata(pszInstanceFname, iCutoff, pszNewInstanceFName);
		} else {
			strcpy(pszNewInstanceFName, pszInstanceFname);
		}

		/*Zhangle_Maxent *pZhangleMaxent = new Zhangle_Maxent(NULL);
		   pZhangleMaxent->fnTrain(pszInstanceFname, "lbfgs", pszModelFname, 100, 2.0);
		   delete pZhangleMaxent;*/

		Tsuruoka_Maxent *pMaxent = new Tsuruoka_Maxent(NULL);
		pMaxent->fnTrain(pszInstanceFname, "l1", pszModelFname, 300);
		delete pMaxent;

		if (strcmp(pszNewInstanceFName, pszInstanceFname) != 0) {
			sprintf(pszNewInstanceFName, "rm %s.tmp", pszInstanceFname);
			system(pszNewInstanceFName);
		}
		delete [] pszNewInstanceFName;
	}


	void fnGetFocusedParentNodes(const SParsedTree* pTree, vector<STreeItem*>& vecFocused){
		for (size_t i = 0; i < pTree->m_vecTerminals.size(); i++) {
			STreeItem *pParent = pTree->m_vecTerminals[i]->m_ptParent;

			while (pParent != NULL) {
				//if (pParent->m_vecChildren.size() > 1 && pParent->m_iEnd - pParent->m_iBegin > 5) {
				if (pParent->m_vecChildren.size() > 1) {
					//do constituent reordering for all children of pParent
					vecFocused.push_back(pParent);
				}
				if (pParent->m_iBrotherIndex != 0) break;
				pParent = pParent->m_ptParent;
			}
		}
	}

	inline void fnGetOutcome(int iL1, int iR1, const SAlignment *pAlign, string& strOutcome) {
		strOutcome = pAlign->fnIsContinuous(iL1, iR1);
	}

	inline string fnGetLengthType(int iLen) {
		if (iLen == 1)
			return string("1");
		if (iLen == 2)
			return string("2");
		if (iLen == 3)
			return string("3");
		if (iLen < 6)
			return string("4");
		if (iLen < 11)
			return string("6");
		return string("11");
	}

	/*
	 * Source side (11 features):
	 * f1: the syntactic category
	 * f2: the syntactic category of its parent
	 * f3: the head word's pos
	 * f4: =1 if it's the head of its parent node
	 *     or
	 *     the head of its parent node
	 * f5: length type
	 */
	void fnGenerateInstance(const SParsedTree *pTree, const STreeItem *pCon1, const SAlignment *pAlign, const vector<string>& vecSTerms, const vector<string>& vecTTerms, string& strOutcome, ostringstream& ostr) {

		fnGetOutcome(pCon1->m_iBegin, pCon1->m_iEnd, pAlign, strOutcome);

		//generate features
		//f1
		ostr << "f1=" << pCon1->m_pszTerm;
		//f2
		ostr << " f2=" << pCon1->m_ptParent->m_pszTerm;
		//f3
		ostr << " f3=" << pTree->m_vecTerminals[pCon1->m_iHeadWord]->m_ptParent->m_pszTerm;
		//f4
		if (pCon1->m_iBrotherIndex == pCon1->m_ptParent->m_iHeadChild) {
			ostr << " f4=1";
		} else {
			ostr << " f4=" << pCon1->m_ptParent->m_vecChildren[pCon1->m_ptParent->m_iHeadChild]->m_pszTerm;
		}
		//f5
		ostr << " f5=" << fnGetLengthType(pCon1->m_iEnd - pCon1->m_iBegin + 1);
	}

	void fnGenerateInstanceFile(const char* pszFlattenedSynFname,        //source-side flattened parse tree file name
                                const char* pszAlignFname,               //alignment filename
                                const char* pszSourceFname,              //source file name
                                const char* pszTargetFname,              //target file name
                                const char* pszInstanceFname             //training instance file name
                                ) {
		SAlignmentReader *pAlignReader = new SAlignmentReader(pszAlignFname);
		SParseReader *pParseReader = new SParseReader(pszFlattenedSynFname, true);
		STxtFileReader *pTxtSReader = new STxtFileReader(pszSourceFname);
		STxtFileReader *pTxtTReader = new STxtFileReader(pszTargetFname);

		FILE *fpOut = fopen(pszInstanceFname, "w");
		assert(fpOut != NULL);

		//read sentence by sentence
		SAlignment *pAlign;
		SParsedTree *pTree;
		char *pszLine = new char[50001];
		int iSentNum = 0;
		while ((pAlign = pAlignReader->fnReadNextAlignment()) != NULL) {
			pTree = pParseReader->fnReadNextParseTree();
			assert(pTree != NULL);
			assert(pTxtSReader->fnReadNextLine(pszLine, NULL));
			vector<string> vecSTerms;
			SplitOnWhitespace(string(pszLine), &vecSTerms);
			assert(pTxtTReader->fnReadNextLine(pszLine, NULL));
			vector<string> vecTTerms;
			SplitOnWhitespace(string(pszLine), &vecTTerms);

			vector<STreeItem*> vecFocused;
			fnGetFocusedParentNodes(pTree, vecFocused);

			for (size_t i = 0; i < vecFocused.size() && pTree->m_vecTerminals.size() > 10; i++) {

				STreeItem *pParent = vecFocused[i];

				for (size_t j = 0; j < pParent->m_vecChildren.size(); j++) {
					//children[j-1] vs. children[j] reordering

					string strOutcome;
					ostringstream ostr;

					fnGenerateInstance(pTree, pParent->m_vecChildren[j], pAlign, vecSTerms, vecTTerms, strOutcome, ostr);

					//fprintf(stderr, "%s %s\n", ostr.str().c_str(), strOutcome.c_str());
					fprintf(fpOut, "%s %s\n", ostr.str().c_str(), strOutcome.c_str());
				}
			}

			delete pAlign;
			delete pTree;
			iSentNum++;

			if (iSentNum % 100000 == 0)
				fprintf(stderr, "#%d\n", iSentNum);
		}


		fclose(fpOut);
		delete pAlignReader;
		delete pParseReader;
		delete pTxtSReader;
		delete pTxtTReader;
		delete [] pszLine;
	}
};

inline void print_options(std::ostream &out,po::options_description const& opts) {
  typedef std::vector< boost::shared_ptr<po::option_description> > Ds;
  Ds const& ds=opts.options();
  out << '"';
  for (unsigned i=0;i<ds.size();++i) {
    if (i) out<<' ';
    out<<"--"<<ds[i]->long_name();
  }
  out << '\n';
}
inline string str(char const* name,po::variables_map const& conf) {
  return conf[name].as<string>();
}

//--parse_file /scratch0/mt_exp/gq-ctb/data/train.srl.cn --align_file /scratch0/mt_exp/gq-ctb/data/aligned.grow-diag-final-and --source_file /scratch0/mt_exp/gq-ctb/data/train.cn --target_file /scratch0/mt_exp/gq-ctb/data/train.en --instance_file /scratch0/mt_exp/gq-ctb/data/srl-instance --model_prefix /scratch0/mt_exp/gq-ctb/data/srl-instance --feature_cutoff 10 --classifier_type 1
int main(int argc, char** argv) {

	po::options_description opts("Configuration options");
	opts.add_options()
                ("parse_file",po::value<string>(),"parse file path (input)")
                ("align_file",po::value<string>(),"Alignment file path (input)")
                ("source_file",po::value<string>(),"Source text file path (input)")
                ("target_file",po::value<string>(),"Target text file path (input)")
                ("instance_file",po::value<string>(),"Instance file path (output)")
                ("model_prefix",po::value<string>(),"Model file path prefix (output): three files will be generated")
                ("classifier_type",po::value<int>()->default_value(1),"Classifier type: 1 for openNLP maxent; 2 for Zhangle maxent; and 3 for SVMLight")
                ("feature_cutoff",po::value<int>()->default_value(100),"Feature cutoff threshold")
                ("svm_option",po::value<string>(),"Parameters for SVMLight classifier")
                ("help", "produce help message");

	po::variables_map vm;
	if (argc) {
		po::store(po::parse_command_line(argc, argv, opts), vm);
		po::notify(vm);
	}

	if (vm.count("help")) {
		print_options(cout, opts);
		return 1;
	}

	if (!vm.count("parse_file")
			|| !vm.count("align_file")
			|| !vm.count("source_file")
			|| !vm.count("target_file")
			|| !vm.count("instance_file")
			|| !vm.count("model_prefix")
			) {
		print_options(cout, opts);
		if (!vm.count("parse_file")) cout << "--parse_file NOT FOUND\n";
		if (!vm.count("align_file")) cout << "--align_file NOT FOUND\n";
		if (!vm.count("source_file")) cout << "--source_file NOT FOUND\n";
		if (!vm.count("target_file")) cout << "--target_file NOT FOUND\n";
		if (!vm.count("instance_file")) cout << "--instance_file NOT FOUND\n";
		if (!vm.count("model_prefix")) cout << "--model_prefix NOT FOUND\n";
		exit(0);
	}

	const char *pOption;
	if (vm.count("svm_option"))
		pOption = str("svm_option", vm).c_str();
	else
		pOption = NULL;

	SConstReorderTrainer *pTrainer = new SConstReorderTrainer(str("parse_file", vm).c_str(),
			                                                  str("align_file", vm).c_str(),
			                                                  str("source_file", vm).c_str(),
			                                                  str("target_file", vm).c_str(),
			                                                  str("instance_file", vm).c_str(),
			                                                  str("model_prefix", vm).c_str(),
			                                                  vm["classifier_type"].as<int>(),
			                                                  vm["feature_cutoff"].as<int>(),
			                                                  pOption);
	delete pTrainer;

	return 1;

}
