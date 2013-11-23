#ifndef _FF_WORDSET_H_
#define _FF_WORDSET_H_

#include "ff.h"
#include "tdict.h"

#include <vector>
#include <string>
#include <iostream>
#include <fstream>

#ifndef HAVE_OLD_CPP
# include <unordered_set>
#else
# include <tr1/unordered_set>
namespace std { using std::tr1::unordered_set; }
#endif

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

 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:

  static void parseArgs(const std::string& args, std::string* featName, std::string* vocabFile, bool* oovMode);
  static void loadVocab(const std::string& vocabFile, std::unordered_set<WordID>* vocab);

  int fid_;
  bool oovMode_;
  std::unordered_set<WordID> vocab_;
};

#endif
