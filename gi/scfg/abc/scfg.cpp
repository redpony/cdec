#include <iostream>
#include <fstream>

#include <boost/shared_ptr.hpp>
#include <boost/pointer_cast.hpp>
#include "lattice.h"
#include "tdict.h"
#include "agrammar.h"
#include "bottom_up_parser.h"
#include "hg.h"
#include "hg_intersect.h"
#include "../utils/ParamsArray.h"


using namespace std;

vector<string> src_corpus;
vector<string> tgt_corpus;

bool openParallelCorpora(string & input_filename){
  ifstream input_file;

  input_file.open(input_filename.c_str());
  if (!input_file) {
    cerr << "Cannot open input file " << input_filename << ". Exiting..." << endl;
    return false;
  } 

  int line =0;
  while (!input_file.eof()) {
    // get a line of source language data                                                                                                                                          
    //    cerr<<"new line "<<ctr<<endl;                                                                                                                                           
    string str;

    getline(input_file, str);
    line++;
    if (str.length()==0){
      cerr<<" sentence number "<<line<<" is empty, skip the sentence\n";
      continue;
    }
    string delimiters("|||");

    vector<string> v = tokenize(str, delimiters);

    if ( (v.size() != 2)  and (v.size() != 3) )  {
      cerr<<str<<endl;
      cerr<<" source or target sentence is not found in sentence number "<<line<<" , skip the sentence\n";
      continue;
    }

    src_corpus.push_back(v[0]);
    tgt_corpus.push_back(v[1]);
  }
  return true;
}


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


  //  cout<<"  Start parse the sentence pairs\n"<<endl;
  Lattice lsource = convertSentenceToLattice(src);
  
  //parse the source sentence by the grammar

  vector<GrammarPtr> grammars(1, g);

  ExhaustiveBottomUpParser parser = ExhaustiveBottomUpParser(goal_sym, grammars);
  
  if (!parser.Parse(lsource, &hg)){

     cerr<<"source sentence is not parsed by the grammar!"<<endl;
     return false;
   }

  //intersect the hg with the target sentence
  Lattice ltarget = convertSentenceToLattice(tgt);

  //forest.PrintGraphviz();
  if (!HG::Intersect(ltarget, & hg)) return false;

  SparseVector<double> reweight;
  
  reweight.set_value(FD::Convert("MinusLogP"), -1 );
  hg.Reweight(reweight);

  return true;
  
}




int main(int argc, char** argv){

  ParamsArray params(argc, argv);
  params.setDescription("scfg models");

  params.addConstraint("grammar_file", "grammar file (default ./grammar.pr )", true); //  optional                               

  params.addConstraint("input_file", "parallel input file (default ./parallel_corpora)", true); //optional                                         

  params.addConstraint("output_file", "grammar output file (default ./grammar_output)", true); //optional                                         

  params.addConstraint("goal_symbol", "top nonterminal symbol (default: X)", true); //optional                                         

  params.addConstraint("split", "split one nonterminal into 'split' nonterminals (default: 2)", true); //optional                                         

  params.addConstraint("prob_iters", "number of iterations (default: 10)", true); //optional                                         

  params.addConstraint("split_iters", "number of splitting iterations (default: 3)", true); //optional                                         

  params.addConstraint("alpha", "alpha (default: 0.1)", true); //optional                                         

  if (!params.runConstraints("scfg")) {
    return 0;
  }
  cerr<<"get parametters\n\n\n";

  string grammar_file = params.asString("grammar_file", "./grammar.pr");

  string input_file = params.asString("input_file", "parallel_corpora");

  string output_file = params.asString("output_file", "grammar_output");

  string goal_sym = params.asString("goal_symbol", "X");

  int max_split = atoi(params.asString("split", "2").c_str());
  
  int prob_iters = atoi(params.asString("prob_iters", "2").c_str());
  int split_iters = atoi(params.asString("split_iters", "1").c_str());
  double alpha = atof(params.asString("alpha", ".001").c_str());

  /////
  cerr<<"grammar_file ="<<grammar_file<<endl;
  cerr<<"input_file ="<< input_file<<endl;
  cerr<<"output_file ="<< output_file<<endl;
  cerr<<"goal_sym ="<< goal_sym<<endl;
  cerr<<"max_split ="<< max_split<<endl;
  cerr<<"prob_iters ="<< prob_iters<<endl;
  cerr<<"split_iters ="<< split_iters<<endl;
  cerr<<"alpha ="<< alpha<<endl;
  //////////////////////////

  cerr<<"\n\nLoad parallel corpus...\n";
  if (! openParallelCorpora(input_file))
    exit(1);

  cerr<<"Load grammar file ...\n";
  aGrammar * agrammar = load_grammar(grammar_file);
  agrammar->SetGoalNT(goal_sym);
  agrammar->setMaxSplit(max_split);
  agrammar->set_alpha(alpha);

  srand(123);

  GrammarPtr g( agrammar);
  Hypergraph hg;

  int data_size = src_corpus.size();
  for (int i =0; i <split_iters; i++){
    
    cerr<<"Split Nonterminals, iteration "<<(i+1)<<endl;
    agrammar->PrintAllRules(output_file+".s" + itos(i+1));
    agrammar->splitAllNonterminals();

    //vector<string> src_corpus;
    //vector<string> tgt_corpus;
    
    for (int j=0; j<prob_iters; j++){
      cerr<<"reset grammar score\n";
      agrammar->ResetScore();
      //      cerr<<"done reset grammar score\n";
      for (int k=0; k <data_size; k++){
	string src = src_corpus[k];
  
	string tgt = tgt_corpus[k];
	cerr <<"parse sentence pair: "<<src<<"  |||  "<<tgt<<endl;

	if (! parseSentencePair(goal_sym, src, tgt, g, hg) ){
	  cerr<<"target sentence is not parsed by the grammar!\n";
	  //return 1;
	  continue;

	} 
	cerr<<"update edge posterior prob"<<endl;
	boost::static_pointer_cast<aGrammar>(g)->UpdateHgProsteriorProb(hg);
	hg.clear();
      }
      boost::static_pointer_cast<aGrammar>(g)->UpdateScore();
    }
    boost::static_pointer_cast<aGrammar>(g)->PrintAllRules(output_file+".e" + itos(i+1));
  }




  

 


  // // agrammar->ResetScore();
  // // agrammar->UpdateScore();
  // if (! parseSentencePair(goal_sym, src, tgt, g, hg) ){
  //   cerr<<"target sentence is not parsed by the grammar!\n";
  //   return 1;

  //  }
  // //   hg.PrintGraphviz();
  //  //hg.clear();

  // agrammar->PrintAllRules();
  // /*split grammar*/
  // cout<<"split NTs\n"; 
  // cerr<<"first of all write all nonterminals"<<endl;
  // // agrammar->printAllNonterminals();
  // cout<<"after split nonterminal"<<endl;
  // agrammar->PrintAllRules();
  // Hypergraph hg1;
  // if (! parseSentencePair(goal_sym, src, tgt,  g, hg1) ){
  //   cerr<<"target sentence is not parsed by the grammar!\n";
  //   return 1;

  // }

  // hg1.PrintGraphviz();
  

  // agrammar->splitNonterminal(15);
  // cout<<"after split nonterminal"<<TD::Convert(15)<<endl;
  // agrammar->PrintAllRules();

  
  /*load training corpus*/


  /*for each sentence pair in training corpus*/
 
  //  forest.PrintGraphviz();
  /*calculate expected count*/
  
}
