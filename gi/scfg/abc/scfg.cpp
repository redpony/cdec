#include "lattice.h"
#include "tdict.h"
#include "agrammar.h"
#include "bottom_up_parser.h"
#include "hg.h"
#include "hg_intersect.h"
#include "../utils/ParamsArray.h"


using namespace std;

typedef aTextGrammar aGrammar;
aGrammar * load_grammar(string & grammar_filename){
  cerr<<"start_load_grammar "<<grammar_filename<<endl;

  aGrammar * test = new aGrammar(grammar_filename);


  return test;
}

Lattice convertSentenceToLattice(const string & str){

  std::vector<WordID> vID;
  TD::ConvertSentence(str , &vID);
  Lattice lsentence;
  lsentence.resize(vID.size());


  for (int i=0; i<vID.size(); i++){

    lsentence[i].push_back( LatticeArc(vID[i], 0.0, 1) );  
  }

  //  if(!lsentence.IsSentence())
  //  cout<<"not a sentence"<<endl;

  return lsentence;

}

bool parseSentencePair(const string & goal_sym, const string & src, const string & tgt,  GrammarPtr & g, Hypergraph &hg){

  Lattice lsource = convertSentenceToLattice(src);
  
  //parse the source sentence by the grammar

  vector<GrammarPtr> grammars(1, g);

  ExhaustiveBottomUpParser parser = ExhaustiveBottomUpParser(goal_sym, grammars);
  
  if (!parser.Parse(lsource, &hg)){

     cerr<<"source sentence does not parse by the grammar!"<<endl;
     return false;
   }

  //intersect the hg with the target sentence
  Lattice ltarget = convertSentenceToLattice(tgt);

  //forest.PrintGraphviz();
  return HG::Intersect(ltarget, & hg);

}




int main(int argc, char** argv){

  ParamsArray params(argc, argv);
  params.setDescription("scfg models");

  params.addConstraint("grammar_file", "grammar file ", true); //  optional                               

  params.addConstraint("input_file", "parallel input file", true); //optional                                         

  if (!params.runConstraints("scfg")) {
    return 0;
  }
  cerr<<"get parametters\n\n\n";

  string input_file = params.asString("input_file", "parallel_corpora");
  string grammar_file = params.asString("grammar_file", "./grammar.pr");


  string src = "el gato .";
  
  string tgt = "the cat .";


  string goal_sym = "X";
  srand(123);
  /*load grammar*/


  aGrammar * agrammar = load_grammar(grammar_file);
  agrammar->SetGoalNT(goal_sym);
  cout<<"before split nonterminal"<<endl;
  GrammarPtr g( agrammar);

  Hypergraph hg;
  if (! parseSentencePair(goal_sym, src, tgt, g, hg) ){
    cerr<<"target sentence is not parsed by the grammar!\n";
    return 1;

   }
   hg.PrintGraphviz();

  if (! parseSentencePair(goal_sym, src, tgt, g, hg) ){
    cerr<<"target sentence is not parsed by the grammar!\n";
    return 1;

   }
   hg.PrintGraphviz();
   //hg.clear();

  if (1==1) return 1;
 
  agrammar->PrintAllRules();
  /*split grammar*/
  cout<<"split NTs\n"; 
  cerr<<"first of all write all nonterminals"<<endl;
  // agrammar->printAllNonterminals();
  agrammar->setMaxSplit(2);
  agrammar->splitNonterminal(4);
  cout<<"after split nonterminal"<<endl;
  agrammar->PrintAllRules();
  Hypergraph hg1;
  if (! parseSentencePair(goal_sym, src, tgt,  g, hg1) ){
    cerr<<"target sentence is not parsed by the grammar!\n";
    return 1;

  }

  hg1.PrintGraphviz();
  

  agrammar->splitNonterminal(15);
  cout<<"after split nonterminal"<<TD::Convert(15)<<endl;
  agrammar->PrintAllRules();

  
  /*load training corpus*/


  /*for each sentence pair in training corpus*/
 
  //  forest.PrintGraphviz();
  /*calculate expected count*/
  
}
