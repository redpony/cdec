#ifndef _AGRAMMAR_H_
#define _AGRAMMAR_H_

#include "grammar.h"

using namespace std;

class aTGImpl;

struct  aTextGrammar : public Grammar {
  aTextGrammar();
  aTextGrammar(const std::string& file);
  void SetMaxSpan(int m) { max_span_ = m; }
  
  virtual const GrammarIter* GetRoot() const;
  void AddRule(const TRulePtr& rule);
  void ReadFromFile(const std::string& filename);
  virtual bool HasRuleForSpan(int i, int j, int distance) const;
  const std::vector<TRulePtr>& GetUnaryRules(const WordID& cat) const;
  
  void setMaxSplit(int max_split);

  void printAllNonterminals() const;
  void addNonterminal(WordID wordID);

  void splitAllNonterminals();
  void splitNonterminal(WordID wordID);

  //  inline  map<WordID, vector<WordID> > & getSplitNonterminals(){return splitNonterminals_;}
  //  map<WordID, vector<WordID> > splitNonterminals_;
  private:
  int max_span_;
  boost::shared_ptr<aTGImpl> pimpl_;
  int max_split_;
  
  map<WordID, int> nonterminals_; //list of nonterminals of the grammar if nonterminals_[WordID] > 0 the nonterminal WordID is found in the grammar



};




#endif
