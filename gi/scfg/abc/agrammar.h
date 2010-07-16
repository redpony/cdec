#ifndef AGRAMMAR_H_
#define AGRAMMAR_H_

#include "grammar.h"
#include "hg.h"


using namespace std;

class aTRule: public TRule{
 public:
 aTRule() : TRule(){ResetScore(0.00000001); }
  aTRule(TRulePtr rule_);

  void ResetScore(double initscore){//cerr<<"Reset Score "<<this->AsString()<<endl;
    sum_scores_.set_value(FD::Convert("Prob"), initscore);}
  void AddProb(double p ){
    //    cerr<<"in AddProb p="<<p<<endl;
    //    cerr<<"prob sumscores ="<<sum_scores_[FD::Convert("Prob")]<<endl;
    sum_scores_.add_value(FD::Convert("Prob"), p);
    //    cerr<<"after AddProb\n";
  }

  void UpdateScore(double sumprob){
    double minuslogp = 0 - log( sum_scores_.value(FD::Convert("Prob")) /sumprob);
    if (sumprob<  sum_scores_.value(FD::Convert("Prob"))){
      cerr<<"UpdateScore sumprob="<<sumprob<< "  sum_scores_.value(FD::Convert(\"Prob\"))="<< sum_scores_.value(FD::Convert("Prob"))<< this->AsString()<<endl;
      exit(1);
    }
    this->scores_.set_value(FD::Convert("MinusLogP"), minuslogp);

  }
 private:
  SparseVector<double> sum_scores_;
};


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
  WordID nt_; //the labelID of the nt (nt_>0);
  
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


  void splitAllNonterminals();

  void PrintAllRules(const string & filename) const;
  void PrintNonterminalRules(WordID nt) const;
  void SetGoalNT(const string & goal_str);

  void ResetScore();

  void UpdateScore();

  void UpdateHgProsteriorProb(Hypergraph & hg);

  void set_alpha(double alpha){alpha_ = alpha;}
 private:

  void RemoveRule(const TRulePtr & rule);
  void RemoveNonterminal(WordID wordID);

  int max_span_;
  int max_split_;
  boost::shared_ptr<aTGImpl> pimpl_;

  map <WordID, vector<TRulePtr> > lhs_rules_;// WordID >0
  map <WordID, vector<NTRule> > nt_rules_; 

  map <WordID, double> sum_probs_;
  map <WordID, double> cnt_rules;

  double alpha_;

  //  map<WordID, vector<WordID> > grSplitNonterminals;
  WordID goalID;
};


#endif
