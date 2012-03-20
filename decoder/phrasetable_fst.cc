#include "phrasetable_fst.h"

#include <cassert>
#include <iostream>
#include <map>

#include <boost/shared_ptr.hpp>

#include "filelib.h"
#include "tdict.h"

using namespace std;

TargetPhraseSet::~TargetPhraseSet() {}
FSTNode::~FSTNode() {}

class TextTargetPhraseSet : public TargetPhraseSet {
 public:
  void AddRule(TRulePtr rule) {
    rules_.push_back(rule);
  }
  const vector<TRulePtr>& GetRules() const {
    return rules_;
  }

 private:
  // all rules must have arity 0
  vector<TRulePtr> rules_;
};

class TextFSTNode : public FSTNode {
 public:
  const TargetPhraseSet* GetTranslations() const { return data.get(); }
  bool HasData() const { return (bool)data; }
  bool HasOutgoingNonEpsilonEdges() const { return !ptr.empty(); } 
  const FSTNode* Extend(const WordID& t) const {
    map<WordID, TextFSTNode>::const_iterator it = ptr.find(t);
    if (it == ptr.end()) return NULL;
    return &it->second;
  }

  void AddPhrase(const string& phrase);

  void AddPassThroughTranslation(const WordID& w, const SparseVector<double>& feats);
  void ClearPassThroughTranslations();
 private:
  vector<WordID> passthroughs;
  boost::shared_ptr<TargetPhraseSet> data;
  map<WordID, TextFSTNode> ptr;
};

#ifdef DEBUG_CHART_PARSER
static string TrimRule(const string& r) {
  size_t start = r.find(" |||") + 5;
  size_t end = r.rfind(" |||");
  return r.substr(start, end - start);
}
#endif

void TextFSTNode::AddPhrase(const string& phrase) {
  vector<WordID> words;
  TRulePtr rule(TRule::CreateRulePhrasetable(phrase));
  if (!rule) {
    static int err = 0;
    ++err;
    if (err > 2) { cerr << "TOO MANY PHRASETABLE ERRORS\n"; exit(1); }
    return;
  }

  TextFSTNode* fsa = this;
  for (int i = 0; i < rule->FLength(); ++i)
    fsa = &fsa->ptr[rule->f_[i]];

  if (!fsa->data)
    fsa->data.reset(new TextTargetPhraseSet);
  static_cast<TextTargetPhraseSet*>(fsa->data.get())->AddRule(rule);
}

void TextFSTNode::AddPassThroughTranslation(const WordID& w, const SparseVector<double>& feats) {
  TextFSTNode* next = &ptr[w];
  // current, rules are only added if the symbol is completely missing as a
  // word starting the phrase.  As a result, it is possible that some sentences
  // won't parse.  If this becomes a problem, fix it here.
  if (!next->data) {
    TextTargetPhraseSet* tps = new TextTargetPhraseSet;
    next->data.reset(tps);
    TRule* rule = new TRule;
    rule->e_.resize(1, w);
    rule->f_.resize(1, w);
    rule->lhs_ = TD::Convert("___PHRASE") * -1;
    rule->scores_ = feats;
    rule->arity_ = 0;
    tps->AddRule(TRulePtr(rule));
    passthroughs.push_back(w); 
  }
}

void TextFSTNode::ClearPassThroughTranslations() {
  for (int i = 0; i < passthroughs.size(); ++i)
    ptr.erase(passthroughs[i]);
  passthroughs.clear();
}

static void AddPhrasetableToFST(istream* in, TextFSTNode* fst) {
  int lc = 0;
  bool flag = false;
  while(*in) {
    string line;
    getline(*in, line);
    if (line.empty()) continue;
    ++lc;
    fst->AddPhrase(line);
    if (lc % 10000 == 0) { flag = true; cerr << '.' << flush; }
    if (lc % 500000 == 0) { flag = false; cerr << " [" << lc << ']' << endl << flush; }
  }
  if (flag) cerr << endl;
  cerr << "Loaded " << lc << " source phrases\n";
}

FSTNode* LoadTextPhrasetable(istream* in) {
  TextFSTNode *fst = new TextFSTNode;
  AddPhrasetableToFST(in, fst);
  return fst;
}

FSTNode* LoadTextPhrasetable(const vector<string>& filenames) {
  TextFSTNode* fst = new TextFSTNode;
  for (int i = 0; i < filenames.size(); ++i) {
    ReadFile rf(filenames[i]);
    cerr << "Reading phrase from " << filenames[i] << endl;
    AddPhrasetableToFST(rf.stream(), fst);
  }
  return fst;
}

FSTNode* LoadBinaryPhrasetable(const string& fname_prefix) {
  (void) fname_prefix;
  assert(!"not implemented yet");
}

