#include <algorithm>
#include <utility>
#include <map>

#include "rule_lexer.h"
#include "filelib.h"
#include "tdict.h"
#include "agrammar.h"
#include "../utils/Util.h"

bool equal(TRulePtr const & rule1, TRulePtr const & rule2){
  if (rule1->lhs_ != rule2->lhs_) return false;
  if (rule1->f_.size() != rule2->f_.size()) return false;
  if (rule1->e_.size() != rule2->e_.size()) return false;

  for (int i=0; i<rule1->f_.size(); i++)
    if (rule1->f_.at(i) != rule2->f_.at(i)) return false;
  for (int i=0; i<rule1->e_.size(); i++)
    if (rule1->e_.at(i) != rule2->e_.at(i)) return false;
  return true;
}

//const vector<TRulePtr> Grammar::NO_RULES;

void aRemoveRule(vector<TRulePtr> & v, const TRulePtr  & rule){ // remove rule from v if found
  for (int i=0; i< v.size(); i++)
    if (equal(v[i], rule )){
      cout<<"erase rule from vector:"<<rule->AsString()<<endl;
      v.erase(v.begin()+i);
    }
}

struct aTextRuleBin : public RuleBin {
  int GetNumRules() const {
    return rules_.size();
  }
  TRulePtr GetIthRule(int i) const {
    return rules_[i];
  }
  void AddRule(TRulePtr t) {
    rules_.push_back(t);
  }
  void RemoveRule(TRulePtr t){
    for (int i=0; i<rules_.size(); i++){
      if (equal(rules_.at(i), t)){
	rules_.erase(rules_.begin() + i);
	//cout<<"IntextRulebin removerulle\n";
	return;
      }
    }
  }
      

  int Arity() const {
    return rules_.front()->Arity();
  }
  void Dump() const {
    for (int i = 0; i < rules_.size(); ++i)
      cerr << rules_[i]->AsString() << endl;
  }
 private:
  vector<TRulePtr> rules_;
};

struct aTextGrammarNode : public GrammarIter {
  aTextGrammarNode() : rb_(NULL) {}
  ~aTextGrammarNode() {
    delete rb_;
  }
  const GrammarIter* Extend(int symbol) const {
    map<WordID, aTextGrammarNode>::const_iterator i = tree_.find(symbol);
    if (i == tree_.end()) return NULL;
    return &i->second;
  }

  const RuleBin* GetRules() const {
    if (rb_) {
      //rb_->Dump();
    }
    return rb_;
  }

  map<WordID, aTextGrammarNode> tree_;
  aTextRuleBin* rb_;
};

struct aTGImpl {
  aTextGrammarNode root_;
};

aTextGrammar::aTextGrammar() : max_span_(10), pimpl_(new aTGImpl) {}
aTextGrammar::aTextGrammar(const string& file) : 
    max_span_(10),
    pimpl_(new aTGImpl) {
  ReadFromFile(file);
}

const GrammarIter* aTextGrammar::GetRoot() const {
  return &pimpl_->root_;
}

void aTextGrammar::SetGoalNT(const string & goal_str){
  goalID = TD::Convert(goal_str);

}
void getNTRule( const TRulePtr & rule, map<WordID, NTRule> & ntrule_map){
  
  NTRule lhs_ntrule(rule, rule->lhs_ * -1);
  ntrule_map[rule->lhs_ * -1] = lhs_ntrule;

  for (int i=0; i< (rule->f_).size(); i++)
    if (ntrule_map.find((rule->f_).at(i) * -1) == ntrule_map.end() && (rule->f_).at(i) <0 ){
        NTRule rhs_ntrule(rule, rule->f_.at(i) * -1);
	ntrule_map[(rule->f_).at(i) *-1] = rhs_ntrule;
    }
  
  
}
void aTextGrammar::AddRule(const TRulePtr& rule) {
  if (rule->IsUnary()) {
    rhs2unaries_[rule->f().front()].push_back(rule);
    unaries_.push_back(rule);
  } else {
    aTextGrammarNode* cur = &pimpl_->root_;
    for (int i = 0; i < rule->f_.size(); ++i)
      cur = &cur->tree_[rule->f_[i]];
    if (cur->rb_ == NULL)
      cur->rb_ = new aTextRuleBin;
    cur->rb_->AddRule(rule);
  }
  
  //add the rule to lhs_rules_
  lhs_rules_[rule->lhs_* -1].push_back(rule);
  
  //add the rule to nt_rules_
  map<WordID, NTRule> ntrule_map;
  getNTRule (rule, ntrule_map);
  for (map<WordID,NTRule>::const_iterator it= ntrule_map.begin(); it != ntrule_map.end(); it++){
    nt_rules_[it->first].push_back(it->second);
  }
}

void aTextGrammar::RemoveRule(const TRulePtr & rule){
  cout<<"Remove rule:  "<<rule->AsString()<<endl;
  if (rule->IsUnary()) {
    aRemoveRule(rhs2unaries_[rule->f().front()], rule);
    aRemoveRule(unaries_, rule);
  } else {
    aTextGrammarNode* cur = &pimpl_->root_;
    for (int i = 0; i < rule->f_.size(); ++i)
      cur = &cur->tree_[rule->f_[i]];
//     if (cur->rb_ == NULL)
//       cur->rb_ = new aTextRuleBin;
    cur->rb_->RemoveRule(rule);
  }

  //remove rules from lhs_rules_
  
  aRemoveRule(lhs_rules_[rule->lhs_ * -1] , rule);

}

void aTextGrammar::RemoveNonterminal(WordID wordID){
  vector<NTRule> rules = nt_rules_[wordID];
//  //  remove the nonterminal from ntrules_
  nt_rules_.erase(wordID);
  for (int i =0; i<rules.size(); i++)
    RemoveRule(rules[i].rule_);

}

void aTextGrammar::setMaxSplit(int max_split){max_split_ = max_split;}



  
void aTextGrammar::AddSplitNonTerminal(WordID nt_old, vector<WordID> & nts){

  vector<NTRule> rules = nt_rules_[nt_old];

  //  cout<<"\n\n\n start add splitting rules"<<endl;

  const double epsilon = 0.001;
  for (int i=0; i<rules.size(); i++){
    NTRule old_rule = rules.at(i);
    vector<int> ntPos = old_rule.ntPos_; //in rule old_rule, ntPos is the positions of nonterminal nt_old
    //we have to substitute each nt in these positions by the list of new nonterminals in the input vector 'nts'
    //there are cnt =size_of(nts)^ size_of(ntPos) possibilities for the substitutions,
    //hence the rules' new probabilities have to divide to cnt also
    //    cout<<"splitting NT in rule "<<old_rule.rule_->AsString()<<endl;

//     cout<<"nt position in the rules"<<endl;
//     for (int j=0; j<ntPos.size();j++) cout<<ntPos[j]<<"  "; cout<<endl;

    int cnt_newrules = pow( nts.size(), ntPos.size() );
    //    cout<<"cnt_newrules="<<cnt_newrules<<endl;

    double log_nts_size = log(nts.size());


    map<WordID, int> cnt_addepsilon; //cnt_addepsilon and cont_minusepsilon to track the number of rules epsilon is added or minus for each lhs nonterminal, ideally we want these two numbers are equal
    map<WordID, int> cnt_minusepsilon; //these two number also use to control the random generated add epsilon/minus epsilon of a new rule
    cnt_addepsilon[old_rule.rule_->lhs_] = 0;
    cnt_minusepsilon[old_rule.rule_->lhs_] = 0;
    for (int j =0; j<nts.size(); j++) {   cnt_addepsilon[nts[j] ] = 0;   cnt_minusepsilon[nts[j] ] = 0;}


    for (int j=0; j<cnt_newrules; j++){ //each j represents a new rule
      //convert j to a vector of size ntPos.size(), each entry in the vector >=0 and <nts.size()
      int mod = nts.size();
      vector <int> j_vector(ntPos.size(), 0); //initiate the vector to all 0
      int j_tmp =j;
      for (int k=0; k<ntPos.size(); k++){
	j_vector[k] = j_tmp % mod;
	j_tmp = (j_tmp - j_vector[k]) / mod;
      }
      //      cout<<"print vector j_vector"<<endl;
      //      for (int k=0; k<ntPos.size();k++) cout<<j_vector[k]<<"  "; cout<<endl;
      //now use the vector to create a new rule
      TRulePtr newrule(new TRule());

      newrule -> e_   = (old_rule.rule_)->e_;
      newrule -> f_ = old_rule.rule_->f_;
      newrule->lhs_ = old_rule.rule_->lhs_;
      newrule -> arity_ = old_rule.rule_->arity_;
      newrule -> scores_ = old_rule.rule_->scores_;

      //      cout<<"end up update score\n";
      if (ntPos[0] == -1){ //update the lhs
	newrule->lhs_ = nts[j_vector[0]] * -1;

	//score has to randomly add/minus a small epsilon to break the balance
	if (nts.size() >1 && ntPos.size() >1){
	  //  cout<<"start to add/minus epsilon"<<endl;
	  if ( cnt_addepsilon[newrule->lhs_] >= cnt_newrules / (2*ntPos.size()) ) //there are enough rules added epsilon, the new rules has to minus epsilon
	    newrule-> scores_ -= epsilon;
	  else if ( cnt_minusepsilon[newrule->lhs_] >= cnt_newrules / (2*ntPos.size()) ) 
	    newrule-> scores_ += epsilon;
	  else{
	    double  random = rand()/RAND_MAX; 
	    if (random > .5){
	      newrule-> scores_ += epsilon;
	      cnt_addepsilon[newrule->lhs_]++;
	    }
	    else{
	      newrule-> scores_ -= epsilon;
	      cnt_minusepsilon[newrule->lhs_]++;
	    }
	  }
	}


	for (int k=1; k<ntPos.size(); k++){//update f_
	  //	  cout<<"ntPos[k]="<<ntPos[k]<<endl;
	  newrule->f_[ntPos[k]] = nts[j_vector[k]] * -1; //update the ntPos[k-1]-th nonterminal in f_ to the j_vector[k] NT in nts
	}
	newrule -> scores_ += (ntPos.size() -1) * log_nts_size;


      }
      else{
	//score has to randomly add/minus a small epsilon to break the balance
	if ( ntPos.size() >0 && nts.size()>1){
	  //	  cout<<"start to add/minus epsilon"<<endl;
	  if ( cnt_addepsilon[newrule->lhs_] >= cnt_newrules / 2 ) //there are enough rules added epsilon, the new rules has to minus epsilon
	    newrule-> scores_ -= epsilon;
	  else if ( cnt_minusepsilon[newrule->lhs_] >= cnt_newrules /2 ) 
	    newrule-> scores_ += epsilon;
	  else{
	    double  random = rand()/RAND_MAX; 
	    if (random > .5){
	      newrule-> scores_ += epsilon;
	      cnt_addepsilon[newrule->lhs_]++;
	    }
	    else{
	      newrule-> scores_ -= epsilon;
	      cnt_minusepsilon[newrule->lhs_]++;
	    }
	  }
	}


	for (int k=0; k<ntPos.size(); k++){ //update f_
	  //	  cout<<"ntPos[k]="<<ntPos[k]<<endl;
	  newrule->f_[ntPos[k]] = nts[j_vector[k]] * -1;
	}
	newrule -> scores_ += ntPos.size() * log_nts_size;
      }
      this->AddRule (newrule);      
    }//add new rules for each grammar rules

  } //iterate through all grammar rules

}


void aTextGrammar::splitNonterminal(WordID wordID){

  //first added the splits nonterminal into the TD dictionary 
  
  string old_str = TD::Convert(wordID); //get the nonterminal label of wordID, the new nonterminals will be old_str+t where t=1..max_split
  
  vector<WordID> v_splits;//split nonterminal wordID into the list of nonterminals in v_splits
  for (int i =0; i< this->max_split_; i++){
    string split_str = old_str + "+" + itos(i);
    WordID splitID = TD::Convert(split_str);
    v_splits.push_back(splitID);

  }
  
  //  grSplitNonterminals[wordID] = v_splits;

  //print split nonterminas of wordID
  //  v_splits = grSplitNonterminals[wordID];
  // cout<<"print split nonterminals\n";
  // for (int i =0; i<v_splits.size(); i++)
  //   cout<<v_splits[i]<<"\t"<<TD::Convert(v_splits[i])<<endl;

  AddSplitNonTerminal(wordID, v_splits);  
  RemoveNonterminal(wordID);

  //  grSplitNonterminals.erase (grSplitNonterminals.find(WordID) );

  if (wordID == goalID){ //add rule X-> X1; X->X2,... if X is the goal NT
    for (int i =0; i<v_splits.size(); i++){
      TRulePtr rule (new TRule());
      rule ->lhs_ = goalID * -1;
      rule ->f_.push_back(v_splits[i] * -1);
      rule->e_.push_back(0);

      rule->scores_.set_value(FD::Convert("MinusLogP"), log(v_splits.size()) );
      AddRule(rule);
    }

  }


}



void aTextGrammar::PrintAllRules() const{
  map<WordID, vector<TRulePtr> >::const_iterator it;
  for (it= lhs_rules_.begin(); it != lhs_rules_.end(); it++){

    vector<TRulePtr> v = it-> second;
    for (int i =0; i< v.size(); i++){
      cout<<v[i]->AsString()<<"\t"<<endl;
    }
  }
}


void aTextGrammar::PrintNonterminalRules(WordID nt) const{
  vector< NTRule > v;   
  map<WordID, vector<NTRule> >::const_iterator mit= nt_rules_.find(nt);
  if (mit == nt_rules_.end())
    return;

  v = mit->second;

  for (vector<NTRule>::const_iterator it = v.begin(); it != v.end(); it++)
    cout<<it->rule_->AsString()<<endl;
}

static void AddRuleHelper(const TRulePtr& new_rule, void* extra) {
  static_cast<aTextGrammar*>(extra)->AddRule(new_rule);
}

void aTextGrammar::ReadFromFile(const string& filename) {
  ReadFile in(filename);
  RuleLexer::ReadRules(in.stream(), &AddRuleHelper, this);
}

bool aTextGrammar::HasRuleForSpan(int i, int j, int distance) const {
  return (max_span_ >= distance);
}

