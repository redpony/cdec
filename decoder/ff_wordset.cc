#include "ff_wordset.h"

#include "hg.h"
#include "fdict.h"
#include "filelib.h"
#include <boost/algorithm/string.hpp>
#include <sstream>
#include <iostream>

using namespace std;

void WordSet::parseArgs(const string& args, string* featName, string* vocabFile, bool* oovMode) {
  vector<string> toks(10);
  boost::split(toks, args, boost::is_any_of(" "));

  *oovMode = false;

  // skip initial feature name
  for(vector<string>::const_iterator it = toks.begin(); it != toks.end(); ++it) {
    if(*it == "-v") {
      *vocabFile = *++it; // copy

    } else if(*it == "-N") {
      *featName = *++it;
    } else if(*it == "--oov") {
       *oovMode = true;
    } else {
       cerr << "Unrecognized argument: " << *it << endl;
       exit(1);
    }
  }

  if(*featName == "") {
    cerr << "featName (-N) not specified for WordSet" << endl;
    exit(1);
  }
  if(*vocabFile == "") {
    cerr << "vocabFile (-v) not specified for WordSet" << endl;
    exit(1);
  }
}

void WordSet::loadVocab(const string& vocabFile, unordered_set<WordID>* vocab) {
  ReadFile rf(vocabFile);
  if (!rf) {
    cerr << "Unable to open file: " << vocabFile; 
    abort();
  }
  string line;
  while (getline(*rf.stream(), line)) {
    boost::trim(line);
    if(line.empty()) continue;
    WordID vocabId = TD::Convert(line);
    vocab->insert(vocabId);
  }
}

void WordSet::TraversalFeaturesImpl(const SentenceMetadata& /*smeta*/ ,
				    const Hypergraph::Edge& edge,
				    const vector<const void*>& /* ant_contexts */,
				    SparseVector<double>* features,
				    SparseVector<double>* /* estimated_features */,
				    void* /* context */) const {
  double addScore = 0.0;
  for(vector<WordID>::const_iterator it = edge.rule_->e_.begin(); it != edge.rule_->e_.end(); ++it) {
    bool inVocab = (vocab_.find(*it) != vocab_.end());
    if(oovMode_ && !inVocab) {
      addScore += 1.0;
    } else if(!oovMode_ && inVocab) {
      addScore += 1.0;
    }
  }
  features->set_value(fid_, addScore);
}

