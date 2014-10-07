/*
 * argument_reorder_model.cc
 *
 *  Created on: Dec 15, 2013
 *      Author: lijunhui
 */

#include <boost/program_options.hpp>
#include <fstream>

#include "argument_reorder_model.h"
#include "utility.h"
#include "tsuruoka_maxent.h"

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

struct SArgumentReorderTrainer{
	SArgumentReorderTrainer(const char* pszSRLFname,        //source-side srl tree file name
				const char* pszAlignFname,               //alignment filename
	            const char* pszSourceFname,              //source file name
	            const char* pszTargetFname,              //target file name
	            const char* pszTopPredicateFname,        //target file name
	            const char* pszInstanceFname,            //training instance file name
	            const char* pszModelFname,              //classifier model file name
	            int iCutoff) {
		fnGenerateInstanceFiles(pszSRLFname, pszAlignFname, pszSourceFname, pszTargetFname, pszTopPredicateFname, pszInstanceFname);

		string strInstanceFname, strModelFname;
		strInstanceFname = string(pszInstanceFname) + string(".left");
		strModelFname = string(pszModelFname) + string(".left");
		fnTraining(strInstanceFname.c_str(), strModelFname.c_str(), iCutoff);
		strInstanceFname = string(pszInstanceFname) + string(".right");
		strModelFname = string(pszModelFname) + string(".right");
		fnTraining(strInstanceFname.c_str(), strModelFname.c_str(), iCutoff);
	}

	~SArgumentReorderTrainer() {

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

		Tsuruoka_Maxent *pMaxent = new Tsuruoka_Maxent(NULL);
		pMaxent->fnTrain(pszNewInstanceFName, "l1", pszModelFname, 300);
		delete pMaxent;

		if (strcmp(pszNewInstanceFName, pszInstanceFname) != 0) {
			sprintf(pszNewInstanceFName, "rm %s.tmp", pszInstanceFname);
			system(pszNewInstanceFName);
		}
		delete [] pszNewInstanceFName;
	}

	void fnGenerateInstanceFiles(const char* pszSRLFname,        //source-side flattened parse tree file name
				const char* pszAlignFname,               //alignment filename
				const char* pszSourceFname,              //source file name
				const char* pszTargetFname,              //target file name
				const char* pszTopPredicateFname,        //top predicate file name (we only consider predicates with 100+ occurrences
				const char* pszInstanceFname             //training instance file name
				) {
		SAlignmentReader *pAlignReader = new SAlignmentReader(pszAlignFname);
		SSrlSentenceReader *pSRLReader = new SSrlSentenceReader(pszSRLFname);
		STxtFileReader *pTxtSReader = new STxtFileReader(pszSourceFname);
		STxtFileReader *pTxtTReader = new STxtFileReader(pszTargetFname);

		Map *pMapPredicate;
		if (pszTopPredicateFname != NULL)
			pMapPredicate = fnLoadTopPredicates(pszTopPredicateFname);
		else
			pMapPredicate = NULL;

		char *pszLine = new char[50001];

		FILE *fpLeftOut, *fpRightOut;
		sprintf(pszLine, "%s.left", pszInstanceFname);
		fpLeftOut = fopen(pszLine, "w");
		sprintf(pszLine, "%s.right", pszInstanceFname);
		fpRightOut = fopen(pszLine, "w");

		//read sentence by sentence
		SAlignment *pAlign;
		SSrlSentence *pSRL;
		SParsedTree *pTree;
		int iSentNum = 0;
		while ((pAlign = pAlignReader->fnReadNextAlignment()) != NULL) {
			pSRL = pSRLReader->fnReadNextSrlSentence();
			assert(pSRL != NULL);
			pTree = pSRL->m_pTree;
			assert(pTxtSReader->fnReadNextLine(pszLine, NULL));
			vector<string> vecSTerms;
			SplitOnWhitespace(string(pszLine), &vecSTerms);
			assert(pTxtTReader->fnReadNextLine(pszLine, NULL));
			vector<string> vecTTerms;
			SplitOnWhitespace(string(pszLine), &vecTTerms);
			//vecTPOSTerms.size() == 0, given the case when an english sentence fails parsing

			if (pTree != NULL) {
				for (size_t i = 0; i < pSRL->m_vecPred.size(); i++) {
					SPredicate *pPred = pSRL->m_vecPred[i];
					if (strcmp(pTree->m_vecTerminals[pPred->m_iPosition]->m_ptParent->m_pszTerm, "VA") == 0) continue;
					string strPred = string(pTree->m_vecTerminals[pPred->m_iPosition]->m_pszTerm);
					if (pMapPredicate != NULL) {
						Map::iterator iter_map = pMapPredicate->find(strPred);
						if (pMapPredicate != NULL && iter_map == pMapPredicate->end())
							continue;
					}

					SPredicateItem *pPredItem = new SPredicateItem(pTree, pPred);

					vector<string> vecStrBlock;
					for (size_t j = 0; j < pPredItem->vec_items_.size(); j++) {
						SSRLItem *pItem1 = pPredItem->vec_items_[j];
						vecStrBlock.push_back(SArgumentReorderModel::fnGetBlockOutcome(pItem1->tree_item_->m_iBegin, pItem1->tree_item_->m_iEnd, pAlign));
					}

					vector<string> vecStrLeftReorderType;
					vector<string> vecStrRightReorderType;
					SArgumentReorderModel::fnGetReorderType(pPredItem, pAlign, vecStrLeftReorderType, vecStrRightReorderType);
					for (int j = 1; j < pPredItem->vec_items_.size(); j++) {
						string strLeftOutcome, strRightOutcome;
						strLeftOutcome = vecStrLeftReorderType[j - 1];
						strRightOutcome = vecStrRightReorderType[j - 1];
						ostringstream ostr;
						SArgumentReorderModel::fnGenerateFeature(pTree, pPred, pPredItem, j, vecStrBlock[j - 1], vecStrBlock[j], ostr);

						//fprintf(stderr, "%s %s\n", ostr.str().c_str(), strOutcome.c_str());
						//fprintf(fpOut, "sentid=%d %s %s\n", iSentNum, ostr.str().c_str(), strOutcome.c_str());
						fprintf(fpLeftOut, "%s %s\n", ostr.str().c_str(), strLeftOutcome.c_str());
						fprintf(fpRightOut, "%s %s\n", ostr.str().c_str(), strRightOutcome.c_str());
					}
				}
			}
			delete pSRL;

			delete pAlign;
			iSentNum++;

			if (iSentNum % 100000 == 0)
				fprintf(stderr, "#%d\n", iSentNum);
		}
		delete [] pszLine;

		fclose(fpLeftOut);
		fclose(fpRightOut);


		delete pAlignReader;
		delete pSRLReader;
		delete pTxtSReader;
		delete pTxtTReader;
	}



	Map* fnLoadTopPredicates(const char* pszTopPredicateFname) {
		if (pszTopPredicateFname == NULL)
			return NULL;

		Map* pMapPredicate  = new Map();
		STxtFileReader *pReader = new STxtFileReader(pszTopPredicateFname);
		char *pszLine = new char[50001];
		int iNumCount = 0;
		while (pReader->fnReadNextLine(pszLine, NULL)) {
			if (pszLine[0] == '#')
				continue;
			char *p = strchr(pszLine, ' ');
			assert(p != NULL);
			p[0] = '\0';
			p++;
			int iCount = atoi(p);
			if (iCount < 100)
				break;
			(*pMapPredicate)[string(pszLine)] = iNumCount++;
		}
		delete pReader;
		delete [] pszLine;
		return pMapPredicate;
	}
};



namespace po = boost::program_options;

inline void print_options(std::ostream &out,po::options_description const& opts) {
  typedef std::vector< boost::shared_ptr<po::option_description> > Ds;
  Ds const& ds=opts.options();
  out << '"';
  for (unsigned i=0;i<ds.size();++i) {
    if (i) out<<' ';
    out<<"--"<<ds[i]->long_name();
  }
  out << '"\n';
}
inline string str(char const* name,po::variables_map const& conf) {
  return conf[name].as<string>();
}

//--srl_file /scratch0/mt_exp/gale-align/gale-align.nw.srl.cn --align_file /scratch0/mt_exp/gale-align/gale-align.nw.al --source_file /scratch0/mt_exp/gale-align/gale-align.nw.cn --target_file /scratch0/mt_exp/gale-align/gale-align.nw.en --instance_file /scratch0/mt_exp/gale-align/gale-align.nw.argreorder.instance --model_prefix /scratch0/mt_exp/gale-align/gale-align.nw.argreorder.model --feature_cutoff 2
//--srl_file /scratch0/mt_exp/gale-ctb/gale-ctb.srl.cn --align_file /scratch0/mt_exp/gale-ctb/gale-ctb.align --source_file /scratch0/mt_exp/gale-ctb/gale-ctb.cn --target_file /scratch0/mt_exp/gale-ctb/gale-ctb.en0 --instance_file /scratch0/mt_exp/gale-ctb/gale-ctb.argreorder.instance --model_prefix /scratch0/mt_exp/gale-ctb/gale-ctb.argreorder.model --feature_cutoff 2
int main(int argc, char** argv) {

	po::options_description opts("Configuration options");
	opts.add_options()
                ("srl_file",po::value<string>(),"srl file path (input)")
                ("align_file",po::value<string>(),"Alignment file path (input)")
                ("source_file",po::value<string>(),"Source text file path (input)")
                ("target_file",po::value<string>(),"Target text file path (input)")
                ("instance_file",po::value<string>(),"Instance file path (output)")
                ("model_prefix",po::value<string>(),"Model file path prefix (output): three files will be generated")
                ("feature_cutoff",po::value<int>()->default_value(100),"Feature cutoff threshold")
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

	if (!vm.count("srl_file")
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

	SArgumentReorderTrainer *pTrainer = new SArgumentReorderTrainer(str("srl_file", vm).c_str(),
			                                                  str("align_file", vm).c_str(),
			                                                  str("source_file", vm).c_str(),
			                                                  str("target_file", vm).c_str(),
			                                                  NULL,
			                                                  str("instance_file", vm).c_str(),
			                                                  str("model_prefix", vm).c_str(),
			                                                  vm["feature_cutoff"].as<int>());
	delete pTrainer;

	return 1;
}
