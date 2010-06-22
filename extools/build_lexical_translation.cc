/*
 * Build lexical translation table from alignment file to use for lexical translation probabilties when scoring a grammar
 *
 * Ported largely from the train-factored-phrase-model.perl script by Philipp Koehn
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

#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std;
using namespace std::tr1;

static const size_t MAX_LINE_LENGTH = 64000000;

int main(int argc, char* argv[]){

  bool DEBUG = false;

  map <WordID, map<WordID, int> > word_translation;
  map <WordID, int> total_foreign;
  map <WordID, int> total_english;

  AnnotatedParallelSentence sent;
  char* buf = new char[MAX_LINE_LENGTH];
  while(cin) 
    {
      cin.getline(buf, MAX_LINE_LENGTH);
      if (buf[0] == 0) continue;
      
      sent.ParseInputLine(buf);
      
      map <WordID, int> foreign_aligned;
      map <WordID, int> english_aligned;

      //iterate over the alignment to compute aligned words
            
      for(int i =0;i<sent.aligned.width();i++)
	{
	  for (int j=0;j<sent.aligned.height();j++)
	    {
	      if (DEBUG) cout << sent.aligned(i,j) << " ";
	      if( sent.aligned(i,j))
		{
		  if (DEBUG) cout << TD::Convert(sent.f[i])  << " aligned to " << TD::Convert(sent.e[j]);
		  //local counts
		  ++foreign_aligned[sent.f[i]];
		  ++english_aligned[sent.e[j]];

		  //global counts
		  ++word_translation[sent.f[i]][sent.e[j]];
		  ++total_foreign[sent.f[i]];
		  ++total_english[sent.e[j]];
		}
	    }
	  if (DEBUG)  cout << endl;
	}
      if (DEBUG) cout << endl;
      
      static const WordID NULL_ = TD::Convert("NULL");
      //handle unaligned words - align them to null
      map<WordID, int>& nullcounts = word_translation[NULL_];
      for (int j =0; j < sent.e_len; j++)
	{
	  if (english_aligned.count(sent.e[j])) continue;
	  ++nullcounts[sent.e[j]];
	  ++total_foreign[NULL_];
	  ++total_english[sent.e[j]];
	}

      for (int i =0; i < sent.f_len; i++)
	{
	  if (foreign_aligned.count(sent.f[i])) continue;
	  ++word_translation[sent.f[i]][NULL_];
	  ++total_english[NULL_];
	  ++total_foreign[sent.f[i]];
	}
      
    }

  for(map < WordID, map<WordID,int> >::iterator it = word_translation.begin(); it != word_translation.end(); ++it)
    {
    const map<WordID, int>& trans = it->second;
    for (map<WordID,int>::const_iterator iit = trans.begin(); iit != trans.end(); ++iit) {
      cout <<  TD::Convert(it->first) <<  "," << TD::Convert(iit->first) << "=" << iit->second << "/" << total_foreign[it->first] << endl;
    }
  }


  return 0;
}
