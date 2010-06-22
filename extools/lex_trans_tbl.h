/*
 * lex_trans_tbl.h
 *
 *  Created on: May 25, 2010
 *      Author: Vlad
 */

#ifndef LEX_TRANS_TBL_H_
#define LEX_TRANS_TBL_H_

#include <map>

class LexTranslationTable
{
 public:

  std::map < std::pair<WordID,WordID>,int > word_translation;
  std::map <WordID, int> total_foreign;
  std::map <WordID, int> total_english;
  void createTTable(const char* buf);
  
};

#endif /* LEX_TRANS_TBL_H_ */
