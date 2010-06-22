/*
 * Score a grammar in striped format
 * ./score_grammar <alignment> < filtered.grammar > scored.grammar
 */
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <cstdlib>
#include <fstream>
#include <tr1/unordered_map>

#include "sentence_pair.h"
#include "extract.h"
#include "fdict.h"
#include "tdict.h"
#include "lex_trans_tbl.h"
#include "filelib.h"

#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std;
using namespace std::tr1;


static const size_t MAX_LINE_LENGTH = 64000000;

typedef unordered_map<vector<WordID>, RuleStatistics, boost::hash<vector<WordID> > > ID2RuleStatistics;


namespace {
  inline bool IsWhitespace(char c) { return c == ' ' || c == '\t'; }
  inline bool IsBracket(char c){return c == '[' || c == ']';}
  inline void SkipWhitespace(const char* buf, int* ptr) {
    while (buf[*ptr] && IsWhitespace(buf[*ptr])) { ++(*ptr); }
  }
}

int ReadPhraseUntilDividerOrEnd(const char* buf, const int sstart, const int end, vector<WordID>* p) {
  static const WordID kDIV = TD::Convert("|||");
  int ptr = sstart;
  while(ptr < end) {
    while(ptr < end && IsWhitespace(buf[ptr])) { ++ptr; }
    int start = ptr;
    while(ptr < end && !IsWhitespace(buf[ptr])) { ++ptr; }
    if (ptr == start) {cerr << "Warning! empty token.\n"; return ptr; }
    const WordID w = TD::Convert(string(buf, start, ptr - start));

    if((IsBracket(buf[start]) and IsBracket(buf[ptr-1])) or( w == kDIV))
      p->push_back(1 * w);
    else {
      if (w == kDIV) return ptr;
      p->push_back(w);
    }
  }
  return ptr;
}


void ParseLine(const char* buf, vector<WordID>* cur_key, ID2RuleStatistics* counts) {
  static const WordID kDIV = TD::Convert("|||");
  counts->clear();
  int ptr = 0;
  while(buf[ptr] != 0 && buf[ptr] != '\t') { ++ptr; }
  if (buf[ptr] != '\t') {
    cerr << "Missing tab separator between key and value!\n INPUT=" << buf << endl;
    exit(1);
  }
  cur_key->clear();
  // key is: "[X] ||| word word word"
  int tmpp = ReadPhraseUntilDividerOrEnd(buf, 0, ptr, cur_key);
  cur_key->push_back(kDIV);
  ReadPhraseUntilDividerOrEnd(buf, tmpp, ptr, cur_key);
  ++ptr;
  int start = ptr;
  int end = ptr;
  int state = 0; // 0=reading label, 1=reading count
  vector<WordID> name;
  while(buf[ptr] != 0) {
    while(buf[ptr] != 0 && buf[ptr] != '|') { ++ptr; }
    if (buf[ptr] == '|') {
      ++ptr;
      if (buf[ptr] == '|') {
        ++ptr;
        if (buf[ptr] == '|') {
          ++ptr;
          end = ptr - 3;
          while (end > start && IsWhitespace(buf[end-1])) { --end; }
          if (start == end) {
            cerr << "Got empty token!\n  LINE=" << buf << endl;
            exit(1);
          }
          switch (state) {
            case 0: ++state; name.clear(); ReadPhraseUntilDividerOrEnd(buf, start, end, &name); break;
            case 1: --state; (*counts)[name].ParseRuleStatistics(buf, start, end); break;
            default: cerr << "Can't happen\n"; abort();
          }
          SkipWhitespace(buf, &ptr);
          start = ptr;
        }
      }
    }
  }
  end=ptr;
  while (end > start && IsWhitespace(buf[end-1])) { --end; }
  if (end > start) {
    switch (state) {
      case 0: ++state; name.clear(); ReadPhraseUntilDividerOrEnd(buf, start, end, &name); break;
      case 1: --state; (*counts)[name].ParseRuleStatistics(buf, start, end); break;
      default: cerr << "Can't happen\n"; abort();
    }
  }
}



void LexTranslationTable::createTTable(const char* buf){

  bool DEBUG = false;

  AnnotatedParallelSentence sent;
      
  sent.ParseInputLine(buf);
      
  //iterate over the alignment to compute aligned words
  
  for(int i =0;i<sent.aligned.width();i++)
    {
      for (int j=0;j<sent.aligned.height();j++)
	{
	  if (DEBUG) cerr << sent.aligned(i,j) << " ";
	  if( sent.aligned(i,j))
	    {
	      if (DEBUG) cerr << TD::Convert(sent.f[i])  << " aligned to " << TD::Convert(sent.e[j]);
	      ++word_translation[pair<WordID,WordID> (sent.f[i], sent.e[j])];
	      ++total_foreign[sent.f[i]];
	      ++total_english[sent.e[j]];
	    }
	}
      if (DEBUG)  cerr << endl;
    }
  if (DEBUG) cerr << endl;
  
  static const WordID NULL_ = TD::Convert("NULL");
  //handle unaligned words - align them to null
  for (int j =0; j < sent.e_len; j++)
    {
      if (sent.e_aligned[j]) continue;
      ++word_translation[pair<WordID,WordID> (NULL_, sent.e[j])];
      ++total_foreign[NULL_];
      ++total_english[sent.e[j]];
    }
  
  for (int i =0; i < sent.f_len; i++)
    {
      if (sent.f_aligned[i]) continue;
      ++word_translation[pair<WordID,WordID> (sent.f[i], NULL_)];
      ++total_english[NULL_];
      ++total_foreign[sent.f[i]];
    }
 
}


inline float safenlog(float v) {
  if (v == 1.0f) return 0.0f;
  float res = -log(v);
  if (res > 100.0f) res = 100.0f;
  return res;
}

int main(int argc, char** argv){
  bool DEBUG= false;
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " corpus.al < filtered.grammar\n";
    return 1;
  }
  ifstream alignment (argv[1]);
  istream& unscored_grammar = cin;
  ostream& scored_grammar = cout;

  //create lexical translation table
  cerr << "Creating table..." << endl;
  char* buf = new char[MAX_LINE_LENGTH];

  LexTranslationTable table;

  while(!alignment.eof())
    {
      alignment.getline(buf, MAX_LINE_LENGTH);
      if (buf[0] == 0) continue;
      
      table.createTTable(buf);      	
    }
  
  bool PRINT_TABLE=false;
  if (PRINT_TABLE)
    {
      ofstream trans_table;
      trans_table.open("lex_trans_table.out");
      for(map < pair<WordID,WordID>,int >::iterator it = table.word_translation.begin(); it != table.word_translation.end(); ++it)
      {
	trans_table <<  TD::Convert(it->first.first) <<  "|||" << TD::Convert(it->first.second) << "==" << it->second << "//" << table.total_foreign[it->first.first] << "//" << table.total_english[it->first.second] << endl;
      } 

      trans_table.close();
    }
  
 
  //score unscored grammar
  cerr <<"Scoring grammar..." << endl;

  ID2RuleStatistics acc, cur_counts;
  vector<WordID> key, cur_key,temp_key;
  vector< pair<short,short> > al;
  vector< pair<short,short> >::iterator ita;
  int line = 0;

  static const int kCF = FD::Convert("CF");
  static const int kCE = FD::Convert("CE");
  static const int kCFE = FD::Convert("CFE");	

  while(!unscored_grammar.eof())
    {
      ++line;
      unscored_grammar.getline(buf, MAX_LINE_LENGTH);
      if (buf[0] == 0) continue;
      ParseLine(buf, &cur_key, &cur_counts);
      
      //loop over all the Target side phrases that this source aligns to
      for (ID2RuleStatistics::const_iterator it = cur_counts.begin(); it != cur_counts.end(); ++it)
	{
	  
	 /*Compute phrase translation prob.
	   Print out scores in this format:
	   Phrase trnaslation prob P(F|E)
	   Phrase translation prob P(E|F)
	   Lexical weighting prob lex(F|E)
	   Lexical weighting prob lex(E|F)
	 */      
	  
	  float pEF_ = it->second.counts.value(kCFE) / it->second.counts.value(kCF);
	  float pFE_ = it->second.counts.value(kCFE) / it->second.counts.value(kCE);

	  map <WordID, pair<int, float> > foreign_aligned;
	  map <WordID, pair<int, float> > english_aligned;

	  //Loop over all the alignment points to compute lexical translation probability
	  al = it->second.aligns;	  
	  for(ita = al.begin(); ita != al.end(); ++ita)
	    {
	     
	      if (DEBUG)
		{
		  cerr << "\nA:" << ita->first << "," << ita->second << "::";
		  cerr <<  TD::Convert(cur_key[ita->first + 2]) << "-" << TD::Convert(it->first[ita->second]);
		}


	      //Lookup this alignment probability in the table
	      int temp = table.word_translation[pair<WordID,WordID> (cur_key[ita->first+2],it->first[ita->second])];
	      float f2e=0, e2f=0;
	      if ( table.total_foreign[cur_key[ita->first+2]] != 0)
		f2e = (float) temp / table.total_foreign[cur_key[ita->first+2]];
	      if ( table.total_english[it->first[ita->second]] !=0 )
		e2f = (float) temp / table.total_english[it->first[ita->second]];
	      if (DEBUG) printf (" %d %E %E\n", temp, f2e, e2f);
	      
	      
	      //local counts to keep track of which things haven't been aligned, to later compute their null alignment	      
	      if (foreign_aligned.count(cur_key[ita->first+2]))
		{
		  foreign_aligned[ cur_key[ita->first+2] ].first++;
		  foreign_aligned[ cur_key[ita->first+2] ].second += e2f;
		}
	      else
		foreign_aligned [ cur_key[ita->first+2] ] = pair<int,float> (1,e2f);
		
	      

	      if (english_aligned.count( it->first[ ita->second] ))
		{
		  english_aligned[ it->first[ ita->second ]].first++;
		  english_aligned[  it->first[ ita->second] ].second += f2e;
		}
	      else
		english_aligned [ it->first[ ita->second] ] = pair<int,float> (1,f2e);
		
	      
	    
	   	      
	    }

	  float final_lex_f2e=1, final_lex_e2f=1;
	  static const WordID NULL_ = TD::Convert("NULL");

	  //compute lexical weight P(F|E) and include unaligned foreign words
	   for(int i=0;i<cur_key.size(); i++)
	     {
	       
	       if (!table.total_foreign.count(cur_key[i])) continue;      //if we dont have it in the translation table, we won't know its lexical weight
	       
	       if (foreign_aligned.count(cur_key[i])) 
		 {
		   pair<int, float> temp_lex_prob = foreign_aligned[cur_key[i]];
		   final_lex_e2f *= temp_lex_prob.second / temp_lex_prob.first;
		 }
	       else //dealing with null alignment
		 {
		   int temp_count = table.word_translation[pair<WordID,WordID> (cur_key[i],NULL_)];
		   float temp_e2f = (float) temp_count / table.total_english[NULL_];
		   final_lex_e2f *= temp_e2f;
		 }	       	       

	     }

	   //compute P(E|F) unaligned english words
	   for(int j=0; j< it->first.size(); j++)
	     {
	       if (!table.total_english.count(it->first[j])) continue;
	       
	       if (english_aligned.count(it->first[j]))
		 {
		   pair<int, float> temp_lex_prob = english_aligned[it->first[j]];
		   final_lex_f2e *= temp_lex_prob.second / temp_lex_prob.first;
		 }
	       else //dealing with null
		 {
		   int temp_count = table.word_translation[pair<WordID,WordID> (NULL_,it->first[j])];
		   float temp_f2e = (float) temp_count / table.total_foreign[NULL_];
		   final_lex_f2e *= temp_f2e;
		 }
	     }
	   
	   
           scored_grammar << TD::GetString(cur_key);
	   scored_grammar << " " << TD::GetString(it->first) << " |||";
	   scored_grammar << " " << safenlog(pFE_) << " " << safenlog(pEF_);
	   scored_grammar << " " << safenlog(final_lex_e2f) << " " << safenlog(final_lex_f2e) << endl;
	}  
    }
}

