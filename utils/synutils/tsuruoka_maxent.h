/*
 * tsuruoka_maxent.h
 *
 */

#ifndef TSURUOKA_MAXENT_H_
#define TSURUOKA_MAXENT_H_

#include "utility.h"
#include "stringlib.h"
#include "maxent-3.0/maxent.h"

#include <assert.h>
#include <vector>
#include <string>
#include <string.h>
#include <tr1/unordered_map>

using namespace std;


typedef std::tr1::unordered_map<std::string, int> Map;
typedef std::tr1::unordered_map<std::string, int>::iterator Iterator;



struct Tsuruoka_Maxent{
	Tsuruoka_Maxent(const char* pszModelFName) {
		if (pszModelFName != NULL) {
			m_pModel = new ME_Model();
			m_pModel->load_from_file(pszModelFName);
		} else
			m_pModel = NULL;
	}

	~Tsuruoka_Maxent() {
		if (m_pModel != NULL)
			delete m_pModel;
	}

	void fnTrain(const char* pszInstanceFName, const char* pszAlgorithm, const char* pszModelFName, int iNumIteration) {
		assert(strcmp(pszAlgorithm, "l1") == 0
				|| strcmp(pszAlgorithm, "l2") == 0
				|| strcmp(pszAlgorithm, "sgd") == 0
				|| strcmp(pszAlgorithm, "SGD") == 0);
		FILE *fpIn = fopen(pszInstanceFName, "r");

		ME_Model *pModel = new ME_Model();

		char *pszLine = new char[100001];
		int iNumInstances = 0;
		int iLen;
		while (!feof(fpIn)) {
			pszLine[0] = '\0';
			fgets(pszLine, 20000, fpIn);
			if (strlen(pszLine) == 0) {
				continue;
			}

			iLen = strlen(pszLine);
			while (iLen > 0 && pszLine[iLen - 1] > 0 && pszLine[iLen -1] < 33) {
				pszLine[ iLen - 1 ] = '\0';
				iLen--;
			}


			iNumInstances++;

			ME_Sample *pmes = new ME_Sample();

			char *p = strrchr(pszLine, ' ');
			assert(p != NULL);
			p[0] = '\0';
			p++;
			vector<string> vecContext;
			SplitOnWhitespace(string(pszLine), &vecContext);

			pmes->label = string(p);
			for (size_t i = 0; i < vecContext.size(); i++)
				pmes->add_feature(vecContext[i]);
			pModel->add_training_sample((*pmes));
			if (iNumInstances % 100000 == 0)
				fprintf(stdout, "......Reading #Instances: %1d\n", iNumInstances);
			delete pmes;
		}
		fprintf(stdout, "......Reading #Instances: %1d\n", iNumInstances);
		fclose(fpIn);

		if (strcmp(pszAlgorithm, "l1") == 0)
			pModel->use_l1_regularizer(1.0);
		else if (strcmp(pszAlgorithm, "l2") == 0)
			pModel->use_l2_regularizer(1.0);
		else
			pModel->use_SGD();

		pModel->train();
		pModel->save_to_file(pszModelFName);

		delete pModel;
		fprintf(stdout, "......Finished Training\n");
		fprintf(stdout, "......Model saved as %s\n", pszModelFName);
		delete [] pszLine;
	}

	double fnEval(const char* pszContext, const char* pszOutcome) const {
		vector<string> vecContext;
		ME_Sample *pmes = new ME_Sample();
		SplitOnWhitespace(string(pszContext), &vecContext);

		for (size_t i = 0; i < vecContext.size(); i++)
			pmes->add_feature(vecContext[i]);
		vector<double> vecProb = m_pModel->classify(*pmes);
		delete pmes;
		int iLableID = m_pModel->get_class_id(pszOutcome);
		return vecProb[iLableID];
	}
	void fnEval(const char* pszContext, vector<pair<string, double> >& vecOutput) const {
		vector<string> vecContext;
		ME_Sample *pmes = new ME_Sample();
		SplitOnWhitespace(string(pszContext), &vecContext);

		vecOutput.clear();

		for (size_t i = 0; i < vecContext.size(); i++)
			pmes->add_feature(vecContext[i]);
		vector<double> vecProb = m_pModel->classify(*pmes);

		for (size_t i = 0; i < vecProb.size(); i++) {
			string label = m_pModel->get_class_label(i);
			vecOutput.push_back(make_pair(label, vecProb[i]));
		}
		delete pmes;
	}
	void fnEval(const char* pszContext, vector<double>& vecOutput) const{
		vector<string> vecContext;
		ME_Sample *pmes = new ME_Sample();
		SplitOnWhitespace(string(pszContext), &vecContext);

		vecOutput.clear();

		for (size_t i = 0; i < vecContext.size(); i++)
			pmes->add_feature(vecContext[i]);
		vector<double> vecProb = m_pModel->classify(*pmes);

		for (size_t i = 0; i < vecProb.size(); i++) {
			string label = m_pModel->get_class_label(i);
			vecOutput.push_back(vecProb[i]);
		}
		delete pmes;
	}
	int fnGetClassId(const string& strLabel) const {
		return m_pModel->get_class_id(strLabel);
	}
private:
	ME_Model *m_pModel;
};



#endif /* TSURUOKA_MAXENT_H_ */
