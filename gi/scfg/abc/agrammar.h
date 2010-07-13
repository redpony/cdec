#ifndef AGRAMMAR_H_
#define AGRAMMAR_H_

#include "grammar.h"


using namespace std;

class aTGImpl;
struct NTRule{

  NTRule(){};
  NTRule(const TRulePtr & rule, WordID nt){
    nt_ = nt;
    rule_ = rule;
    
    if (rule->lhs_ * -1 == nt) 
      ntPos_.push_back(-1);
    
    for (int i=0; i< rule->f().size(); i++)
      if (rule->f().at(i) * -1 == nt)
	ntPos_.push_back(i);
  }
  
  TRulePtr rule_;
  WordID nt_; //the labelID of the nt (WordID>0);
  
  vector<int> ntPos_; //position of nt_ -1: lhs, from 0...f_.size() for nt of f_()
  //i.e the rules is: NP-> DET NP; if nt_=5 is the labelID of NP then ntPos_ = (-1, 1): the indexes of nonterminal NP
  

};

struct aTextGrammar : public Grammar {
  aTextGrammar();
  aTextGrammar(const std::string& file);
  void SetMaxSpan(int m) { max_span_ = m; }
  
  virtual const GrammarIter* GetRoot() const;
  void AddRule(const TRulePtr& rule);
  void ReadFromFile(const std::string& filename);
  virtual bool HasRuleForSpan(int i, int j, int distance) const;
  const std::vector<TRulePtr>& GetUnaryRules(const WordID& cat) const;

  void AddSplitNonTerminal(WordID nt_old, vector<WordID> & nts);
  void setMaxSplit(int max_split);
  void splitNonterminal(WordID wordID);

  void PrintAllRules() const;
  void PrintNonterminalRules(WordID nt) const;
  void SetGoalNT(const string & goal_str);
 private:

  void RemoveRule(const TRulePtr & rule);
  void RemoveNonterminal(WordID wordID);

  int max_span_;
  int max_split_;
  boost::shared_ptr<aTGImpl> pimpl_;
  map <WordID, vector<TRulePtr> > lhs_rules_;// WordID >0
  map <WordID, vector<NTRule> > nt_rules_; 

  //  map<WordID, vector<WordID> > grSplitNonterminals;
  WordID goalID;
};


#endif
