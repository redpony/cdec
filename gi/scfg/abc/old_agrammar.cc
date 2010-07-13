#include "agrammar.h"
#include "Util.h"

#include <algorithm>
#include <utility>
#include <map>

#include "rule_lexer.h"
#include "filelib.h"
#include "tdict.h"
#include <iostream>
#include <fstream>

map<WordID, vector<WordID> > grSplitNonterminals;
//const vector<TRulePtr> Grammar::NO_RULES;


// vector<TRulePtr> substituteF(TRulePtr & rule, WordID wordID, vector<WordID> & v){
//   vector<TRulePtr> vRules; //outputs

//   vector<WordID> f = rule->f();
//   vector<vector<WordID> > newfvector;
//   for (int i =0; i< f.size(); i++){
//     if (f[i] == wordID){
//       newfvector.push_back(v);
//     }
//     else
//       newfvector.push_back(vector<WordID> (1, f[i]));
//   }
  
//   //now creates new rules;


//   return vRules;
// }


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
  int Arity() const {
    return rules_.front()->Arity();
  }
  void Dump() const {
    for (int i = 0; i < rules_.size(); ++i)
      cerr << rules_[i]->AsString() << endl;
  }


  vector<TRulePtr> getRules(){ return rules_;}


  void substituteF(vector<WordID> & f_path,   map<WordID, vector<WordID> > &   grSplitNonterminals){
    //this substituteF method is different with substituteF procedure found in cdec code;
  //
  //aTextRuleBin has a collection of rules with the same f() on the rhs, 
  //substituteF() replaces the f_ of all the rules with f_path vector, 
  //the grSplitNonterminals input to split the lhs_ nonterminals of the rules  incase the lhs_ nonterminal found in grSplitNonterminals

    vector <TRulePtr> newrules;
    for (vector<TRulePtr>::iterator it = rules_.begin() ; it != rules_.end(); it++){
      assert(f_path.size() == (*it)->f_.size());
      
      if (grSplitNonterminals.find( (*it)->lhs_) == grSplitNonterminals.end()){
	(*it)->f_ = f_path;
      }
      else{ // split the lhs NT, 
	vector<WordID> new_lhs = grSplitNonterminals[ (*it)->lhs_ ];
	for (vector<WordID>::iterator vit = new_lhs.begin(); vit != new_lhs.end(); vit++){
	  TRulePtr newrule;
	  newrule -> e_ = (*it)->e_;
	  newrule -> f_ = (*it)->f_;
	  newrule->lhs_ = *vit;
	  newrule -> scores_ = (*it)->scores_;
	  newrule -> arity_ = (*it)->arity_;
	  newrules.push_back (newrule);
	}
	rules_.erase(it);
      }
    }

    //now add back newrules(output of splitting lhs_) to rules_
    rules_.insert(newrules.begin(),newrules.begin(), newrules.end());
  }
  
private:
  vector<TRulePtr> rules_;
};



struct aTextGrammarNode : public GrammarIter {
  aTextGrammarNode() : rb_(NULL) {}

  aTextGrammarNode(const aTextGrammarNode  & a){
    nonterminals_ = a.nonterminals_;
    tree_ = a.tree_;
    rb_  = new  aTextRuleBin(); //cp constructor: don't cp the set of rules over 
  }

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
 
  void DFS();

  void visit (); //todo: make this as a function pointer

  vector <WordID > path_; //vector of f_ nonterminals/terminals from the top to the current node;
  set<WordID> nonterminals_; //Linh added: the set of nonterminals extend the current TextGrammarNode, WordID  is the label in the dict; i.e WordID>0 
  map<WordID, aTextGrammarNode> tree_;
  aTextRuleBin* rb_;
  
  void print_path(){ //for debug only
    cout<<"path="<<endl;
    for (int i =0; i< path_.size(); i++)
      cout<<path_[i]<<"  ";
    cout<<endl;
  }
};

void aTextGrammarNode::DFS(){ //because the grammar is a tree without circle, DFS does not require to color the nodes

  visit();
  
  for (map<WordID, aTextGrammarNode>::iterator it = tree_.begin(); it != tree_.end(); it++){
    (it->second).DFS();
  }
}


void aTextGrammarNode::visit( ){  

  cout<<"start visit()"<<endl;
  
  cout<<"got grSplitNonterminals"<<endl;
//   if (grSplitNonterminals.find(*it) != grSplitNonterminals.end()){ //split this *it nonterminal
//     vector<WordID> vsplits = grSplitNonterminals[*it]; //split *it into vsplits

  //iterate through next terminals/nonterminals in tree_
  vector<WordID> tobe_removedNTs; //the list of nonterminal children in tree_ were splited hence will be removed from tree_

  for (map<WordID, aTextGrammarNode>::iterator it = tree_.begin() ; it != tree_.end(); it++){
    cout<<"in visit(): inside for loop: wordID=="<<it->first<<endl;

    map<WordID, vector<WordID> >::const_iterator git = grSplitNonterminals.find(it->first * -1 );

    if (git == grSplitNonterminals.end() || it->first >0){ //the next symbols is not to be split
      cout<<"not split\n";
      tree_[it->first ].path_  = path_;
      tree_[it->first ].path_.push_back(it->first);
      cout<<"in visit() tree_[it->first ].path_= ";
      tree_[it->first ].print_path();
      continue;
    }


    cout<<"tmp2";
    vector<WordID> vsplits = grSplitNonterminals[it->first * -1];
    //    vector<WordID> vsplits = git->second;
    cout<<"tmp3";
    //    vector<WordID> vsplits = agrammar_ ->splitNonterminals_[it->first * -1];
    cout <<"got vsplits"<<endl;
    for (int i =0 ; i<vsplits.size(); i++){
      //  nonterminals_.insert(vsplits[i]); //add vsplits[i] into nonterminals_ of the current TextGrammarNode
      tree_[vsplits[i] * -1] = aTextGrammarNode(tree_[it->first]); //cp the subtree to new nonterminal
      tree_[vsplits[i] * -1].path_  = path_; //update the path if the subtrees
      tree_[vsplits[i] * -1].path_.push_back(vsplits[i] * -1);
      tree_[vsplits[i] * -1].print_path();
    }

    //remove the old node:
    tobe_removedNTs.push_back(it->first); 
    
  }

  for (int i =0; i<tobe_removedNTs.size(); i++)
    tree_.erase(tobe_removedNTs[i]);
  
  if (tree_.size() ==0){ //the last (terminal/nonterminal
    cout<<"inside visit(): the last terminal/nonterminal"<<endl;
    rb_->substituteF(path_, grSplitNonterminals);
    
  }
  cout<<"visit() end"<<endl;
}

struct aTGImpl {
  aTextGrammarNode root_;
};

aTextGrammar::aTextGrammar() : max_span_(10), pimpl_(new aTGImpl) {}
aTextGrammar::aTextGrammar(const std::string&  file) : 
  max_span_(10),
  pimpl_(new aTGImpl) {
  ReadFromFile(file);
}


const GrammarIter* aTextGrammar::GetRoot() const {
  return &pimpl_->root_;
}


void aTextGrammar::addNonterminal(WordID wordID){ 
  //addNonterminal add the nonterminal wordID (wordID<0) to the list of nonterminals (map<WordID, int>) nonterminals_ of grammar
  //if the input parameter wordID<0 then do nothing

  if (wordID <0){ //it is a nonterminal

    map<WordID, int>::iterator it = nonterminals_.find(wordID * -1);
    if (it == nonterminals_.end()) //if not found in the list of nonterminals(a new nonterminals)
        nonterminals_[wordID * -1] = 1;
  }
}



void aTextGrammar::AddRule(const TRulePtr& rule) {
  //add the LHS nonterminal to nonterminals_ map

  this->addNonterminal(rule->lhs_);

  if (rule->IsUnary()) {
    rhs2unaries_[rule->f().front()].push_back(rule);
    unaries_.push_back(rule);
    if (rule->f().front() <0)
      //add the RHS nonterminal to the list of nonterminals (the addNonterminal() function will check if it is the rhs symbol is a  nonterminal then multiply by -1)
      this->addNonterminal(rule->f().front());
    
    
  } else {
    aTextGrammarNode* cur = &pimpl_->root_;
    for (int i = 0; i < rule->f_.size(); ++i){
      if (rule->f_[i] <0){ 
	cur->nonterminals_.insert(rule->f_[i] * -1); //add the next(extend) nonterminals to the current node's nonterminals_ set
	this->addNonterminal(rule->f_[i]); //add the rhs nonterminal to the  grammar's list of nonterminals
      }
      cur = &cur->tree_[rule->f_[i]];

    }
    if (cur->rb_ == NULL)
      cur->rb_ = new aTextRuleBin;
    cur->rb_->AddRule(rule);
    
  }
}

static void aAddRuleHelper(const TRulePtr& new_rule, void* extra) {
  static_cast<aTextGrammar*>(extra)->AddRule(new_rule);
}


void aTextGrammar::ReadFromFile(const string& filename) {
  ReadFile in(filename);
  RuleLexer::ReadRules(in.stream(), &aAddRuleHelper, this);
}

bool aTextGrammar::HasRuleForSpan(int i, int j, int distance) const {
  return (max_span_ >= distance);
}


////Linh added

void aTextGrammar::setMaxSplit(int max_split){max_split_ = max_split;}


void aTextGrammar::printAllNonterminals() const{
  for (map<WordID, int>::const_iterator it =nonterminals_.begin();
       it != nonterminals_.end(); it++){
    if (it->second >0){
      cout <<it->first<<"\t"<<TD::Convert(it->first)<<endl;
    }
  }
  
}
 

void aTextGrammar::splitNonterminal(WordID wordID){

  //first added the splits nonterminal into the TD dictionary 
  
  string old_str = TD::Convert(wordID); //get the nonterminal label of wordID, the new nonterminals will be old_str+t where t=1..max_split
  
  vector<WordID> v_splits;//split nonterminal wordID into the list of nonterminals in v_splits
  for (int i =0; i< this->max_split_; i++){
    string split_str = old_str + "+" + itos(i);
    WordID splitID = TD::Convert(split_str);
    v_splits.push_back(splitID);
    nonterminals_[splitID] = 1;
  }
  
  grSplitNonterminals[wordID] = v_splits;
  //set wordID to be an inactive nonterminal
  nonterminals_[wordID] = 0;

  //print split nonterminas of wordID
  v_splits = grSplitNonterminals[wordID];
  cout<<"print split nonterminals\n";
  for (int i =0; i<v_splits.size(); i++)
    cout<<v_splits[i]<<"\t"<<TD::Convert(v_splits[i])<<endl;


  //now update in grammar rules and gramar tree:
  vector<TRulePtr> newrules;
  //first unary rules:
  //iterate through unary rules
  for (int i =0; i < unaries_.size(); i++){
    TRulePtr rule = unaries_[i];
    WordID lhs = rule.lhs_;
    if (grSplitNonterminals.find(rule->f().front() ) != grSplitNonterminals.end()//if the rhs is in the list of splitting nonterminal
	&& grSplitNonterminals.find(lhs ) != grSplitNonterminals.end() //and the lhs is in the list of splitting nonterminal too
	){ 
      vector<WordID> rhs_nonterminals = grSplitNonterminals[rule->f().front()]; //split the  rhs nonterminal into the list of nonterminals in 'rhs_nonterminals'
      vector<WordID> lhs_nonterminals = grSplitNonterminals[lhs]; //split the  rhs nonterminal into the list of nonterminals in 'lhs_nonterminals'      
      for (int k =0; k <rhs_nonterminals.size(); k++)
	for (int j =0; j <lhs_nonterminals.size(); j++){
	  TRulePtr newrule;
	  newrule -> e_ = rule->e_;
	  newrule -> f_ = rhs_nonterminals[k]->f_;
	  newrule->lhs_ = lhs_nonterminals[j]->lhs_;
	  newrule -> scores_ = rule->scores_;
	  newrule -> arity_ = (*it)->arity_;
	  newrules.push_back (newrule);
	  
	  //update 
	}
    }
    else{//the rhs terminal/nonterminal is not in the list of splitting nonterminal
      

    }
  }
  
  // for (Cat2Rule::const_iterator it = rhs2unaries_.begin(); it != rhs2unaries_.end(); it++){

  // }  
  // if (rule->IsUnary()) {
  //   rhs2unaries_[rule->f().front()].push_back(rule);
  //   unaries_.push_back(rule);
  //   if (rule->f().front() <0)
  //     //add the RHS nonterminal to the list of nonterminals (the addNonterminal() function will check if it is the rhs symbol is a  nonterminal then multiply by -1)
  //     this->addNonterminal(rule->f().front());
    

  pimpl_->root_.DFS();
  
}


// void aTextGrammar::splitNonterminal0(WordID wordID){

//   TextGrammarNode* cur = &pimpl_->root_;
//   for (int i = 0; i < rule->f_.size(); ++i)
//     cur = &cur->tree_[rule->f_[i]];
 
// }

void aTextGrammar::splitAllNonterminals(){


}

