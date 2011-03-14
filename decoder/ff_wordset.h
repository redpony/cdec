#ifndef _FF_WORDSET_H_
#define _FF_WORDSET_H_

#include "ff.h"

#include <boost/unordered/unordered_set.hpp>
#include <boost/algorithm/string.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <fstream>

class WordSet : public FeatureFunction {
 public:
// we depend on the order of the initializer list
// to call member constructurs in the proper order
// modify this carefully!
//
// Usage: "WordSet -v vocab.txt [--oov]"
 WordSet(const std::string& param) {
    std::string vocabFile;
    std::string featName;
    parseArgs(param, &featName, &vocabFile, &oovMode_);

    fid_ = FD::Convert(featName);

    std::cerr << "Loading vocab for " << param << " from " << vocabFile << std::endl;
    loadVocab(vocabFile, &vocab_);
  }

  ~WordSet() {
  }

  Features features() const { return single_feature(fid_); }

 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:

  static void loadVocab(const std::string& vocabFile, boost::unordered_set<WordID>* vocab) {

      std::ifstream file;
      std::string line;

      file.open(vocabFile.c_str(), std::fstream::in);
      if (file.is_open()) {
	unsigned lineNum = 0;
	while (!file.eof()) {
	  ++lineNum;
	  getline(file, line);
	  boost::trim(line);
	  if(line.empty()) {
	    continue;
	  }
	  
	  WordID vocabId = TD::Convert(line);
	  vocab->insert(vocabId);
	}
	file.close();
      } else {
	std::cerr << "Unable to open file: " << vocabFile; 
	exit(1);
      }
  }

  static void parseArgs(const std::string& args, std::string* featName, std::string* vocabFile, bool* oovMode) {

    std::vector<std::string> toks(10);
    boost::split(toks, args, boost::is_any_of(" "));

    *oovMode = false;

    // skip initial feature name
    for(std::vector<std::string>::const_iterator it = toks.begin(); it != toks.end(); ++it) {
      if(*it == "-v") {
	*vocabFile = *++it; // copy

      } else if(*it == "-N") {
	*featName = *++it;

      } else if(*it == "--oov") {
	*oovMode = true;

      } else {
	std::cerr << "Unrecognized argument: " << *it << std::endl;
	exit(1);
      }
    }

    if(*featName == "") {
      std::cerr << "featName (-N) not specified for WordSet" << std::endl;
      exit(1);
    }
    if(*vocabFile == "") {
      std::cerr << "vocabFile (-v) not specified for WordSet" << std::endl;
      exit(1);
    }
  }

  int fid_;
  bool oovMode_;
  boost::unordered_set<WordID> vocab_;
};

#endif
